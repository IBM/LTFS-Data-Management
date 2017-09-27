#include <sys/types.h>
#include <signal.h>
#include <sys/stat.h>
#include <sys/mount.h>
#include <linux/fs.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/xattr.h>
#include <string.h>
#include <limits.h>
#include <sys/resource.h>
#include <sys/file.h>
#include <errno.h>

#include <fuse.h>
#include <string>
#include <sstream>
#include <atomic>
#include <typeinfo>
#include <map>
#include <set>
#include <vector>
#include <atomic>
#include <condition_variable>
#include <thread>
#include <mutex>
#include <exception>

#include "src/common/exception/OpenLTFSException.h"
#include "src/common/util/util.h"
#include "src/common/messages/Message.h"
#include "src/common/tracing/Trace.h"
#include "src/common/errors/errors.h"
#include "src/common/const/Const.h"

#include "src/common/comm/ltfsdm.pb.h"
#include "src/common/comm/LTFSDmComm.h"

#include "src/connector/Connector.h"
#include <src/connector/fuse/ltfsdmd.ofs.h>

std::atomic<bool> Connector::connectorTerminate(false);
std::atomic<bool> Connector::forcedTerminate(false);
std::atomic<bool> Connector::recallEventSystemStopped(false);

namespace FuseConnector {
std::mutex mtx;
std::map<std::string, FuseFS*> managedFss;
std::unique_lock<std::mutex> *trecall_lock;
}
;

LTFSDmCommServer recrequest(Const::RECALL_SOCKET_FILE);

Connector::Connector(bool cleanup_) :
        cleanup(cleanup_)

{
    clock_gettime(CLOCK_REALTIME, &starttime);
    FuseFS::ltfsdmKey = LTFSDM::getkey();
}

Connector::~Connector()

{
    try {
        std::string mountpt;

        if (cleanup)
            MSG(LTFSDMF0025I);

        for (auto const& fn : FuseConnector::managedFss) {
            mountpt = fn.second->getMountPoint();
            FuseConnector::managedFss.erase(mountpt);
            delete (fn.second);
        }
        if (cleanup)
            MSG(LTFSDMF0027I);
    } catch (...) {
        kill(getpid(), SIGTERM);
    }
}

void Connector::initTransRecalls()

{
    try {
        recrequest.listen();
    } catch (const std::exception& e) {
        TRACE(Trace::error, e.what());
        MSG(LTFSDMF0026E);
        THROW(Const::UNSET);
    }
}

void Connector::endTransRecalls()

{
    recrequest.closeRef();
}

Connector::rec_info_t Connector::getEvents()

{
    Connector::rec_info_t recinfo;
    long key;

    recrequest.accept();

    try {
        recrequest.recv();
    } catch (const std::exception& e) {
        MSG(LTFSDMF0019E, e.what(), errno);
        THROW(Error::LTFSDM_GENERAL_ERROR);
    }

    const LTFSDmProtocol::LTFSDmTransRecRequest request =
            recrequest.transrecrequest();

    key = request.key();
    if (FuseFS::ltfsdmKey != key) {
        TRACE(Trace::error, (long ) FuseFS::ltfsdmKey, key);
        recinfo = (rec_info_t ) { NULL, false, 0, 0, 0, "" };
        return recinfo;
    }

    struct conn_info_t *conn_info = new struct conn_info_t;
    conn_info->reqrequest = new LTFSDmCommServer(recrequest);

    recinfo.conn_info = conn_info;
    recinfo.toresident = request.toresident();
    recinfo.fsid = request.fsid();
    recinfo.igen = request.igen();
    recinfo.ino = request.ino();
    recinfo.filename = request.filename();

    return recinfo;
}

void Connector::respondRecallEvent(rec_info_t recinfo, bool success)

{
    LTFSDmProtocol::LTFSDmTransRecResp *trecresp =
            recinfo.conn_info->reqrequest->mutable_transrecresp();

    trecresp->set_success(success);

    try {
        recinfo.conn_info->reqrequest->send();
    } catch (const std::exception& e) {
        TRACE(Trace::error, e.what());
        MSG(LTFSDMS0007E);
    }

    TRACE(Trace::always, recinfo.filename, success);

    recinfo.conn_info->reqrequest->closeAcc();

    delete (recinfo.conn_info->reqrequest);
    delete (recinfo.conn_info);
}

void Connector::terminate()

{
    Connector::connectorTerminate = true;

    LTFSDmCommClient commCommand(Const::RECALL_SOCKET_FILE);

    try {
        commCommand.connect();
    } catch (const std::exception& e) {
        MSG(LTFSDMF0020E, e.what(), errno);
        THROW(Error::LTFSDM_GENERAL_ERROR);
    }

    LTFSDmProtocol::LTFSDmTransRecRequest *recrequest =
            commCommand.mutable_transrecrequest();

    recrequest->set_key(FuseFS::ltfsdmKey);
    recrequest->set_toresident(false);
    recrequest->set_fsid(0);
    recrequest->set_igen(0);
    recrequest->set_ino(0);
    recrequest->set_filename("");

    try {
        commCommand.send();
    } catch (const std::exception& e) {
        MSG(LTFSDMF0024E);
        THROW(errno, errno);
    }
}

FsObj::FsObj(std::string fileName) :
        handle(NULL), handleLength(0), isLocked(false), handleFree(true)

{
    FuseFS::FuseHandle *fh = new FuseFS::FuseHandle();
    std::stringstream spath;
    int fd;

    memset((void *) fh, 0, sizeof(FuseFS::FuseHandle));

    if ((fd = open(fileName.c_str(), O_RDWR)) == -1) {
        if (errno == EISDIR) {
            strncpy(fh->mountpoint, fileName.c_str(), PATH_MAX - 1);
            fh->fd = Const::UNSET;
        } else {
            TRACE(Trace::error, errno);
            THROW(errno, fileName, errno);
        }
    } else {
        if (ioctl(fd, FuseFS::LTFSDM_FINFO, fh) == -1) {
            TRACE(Trace::error, errno);
            THROW(errno, fileName, errno);
        } else {
            auto search = FuseConnector::managedFss.find(fh->mountpoint);
            if (search == FuseConnector::managedFss.end()) {
                spath << fh->mountpoint << "/" << fh->fusepath;
                if ((fh->fd = open(spath.str().c_str(), O_RDWR)) == -1) {
                    TRACE(Trace::error, errno);
                    THROW(errno, fileName, errno);
                }
            } else {
                if ((fh->fd = openat(search->second->rootFd, fh->fusepath,
                        O_RDWR)) == -1) {
                    TRACE(Trace::error, errno);
                    THROW(errno, fileName, errno);
                }
            }
        }
        close(fd);
    }
    handle = (void *) fh;
    handleLength = sizeof(FuseFS::FuseHandle);
}

FsObj::FsObj(Connector::rec_info_t recinfo) :
        FsObj::FsObj(recinfo.filename)
{
}

FsObj::~FsObj()

{
    try {
        FuseFS::FuseHandle *fh = (FuseFS::FuseHandle *) handle;

        if (fh->fd != Const::UNSET)
            close(fh->fd);

        delete (fh);
    } catch (...) {
        kill(getpid(), SIGTERM);
    }
}

bool FsObj::isFsManaged()

{
    int val;
    FuseFS::FuseHandle *fh = (FuseFS::FuseHandle *) handle;
    std::unique_lock<std::mutex> lock(FuseConnector::mtx);

    if (getxattr(fh->mountpoint, Const::OPEN_LTFS_EA_MANAGED.c_str(), &val,
            sizeof(val)) == -1) {
        TRACE(Trace::always, fh->mountpoint);
        return false;
    }

    return (val == 1);
}

void FsObj::manageFs(bool setDispo, struct timespec starttime,
        std::string mountPoint, std::string fsName)

{
    FuseFS::FuseHandle *fh = (FuseFS::FuseHandle *) handle;
    FuseFS *FS = nullptr;

    TRACE(Trace::always, fh->mountpoint);

    FS = new FuseFS(fh->mountpoint);

    try {
        FS->init(starttime);
    } catch (const std::exception& e) {
        TRACE(Trace::error, e.what());
        delete (FS);
        THROW(Error::LTFSDM_GENERAL_ERROR);
    }

    std::unique_lock<std::mutex> lock(FuseConnector::mtx);

    FuseConnector::managedFss[fh->mountpoint] = FS;

}

struct stat FsObj::stat()

{
    struct stat statbuf;
    FuseFS::mig_info miginfo;

    FuseFS::FuseHandle *fh = (FuseFS::FuseHandle *) handle;
    std::string sourcemp = std::string(fh->mountpoint).append(
            Const::OPEN_LTFS_CACHE_MP);

    miginfo = FuseFS::getMigInfoAt(fh->fd);

    if (miginfo.state != FuseFS::mig_info::state_num::NO_STATE
            && miginfo.state != FuseFS::mig_info::state_num::IN_MIGRATION) {
        statbuf = miginfo.statinfo;
    } else {
        memset(&statbuf, 0, sizeof(statbuf));
        if (fstat(fh->fd, &statbuf) == -1) {
            TRACE(Trace::error, errno);
            THROW(errno, errno, fh->fusepath);
        }
    }

    return statbuf;
}

unsigned long long FsObj::getFsId()

{
    unsigned long long fsid = 0;
    struct stat statbuf;
    FuseFS::FuseHandle *fh = (FuseFS::FuseHandle *) handle;

    memset(&statbuf, 0, sizeof(statbuf));
    if (fstat(fh->fd, &statbuf) == -1) {
        TRACE(Trace::error, errno);
        THROW(errno, errno, fh->fusepath);
    }

    // TODO
    fsid = statbuf.st_dev;

    return fsid;
}

unsigned int FsObj::getIGen()

{
    unsigned int igen = 0;
    FuseFS::FuseHandle *fh = (FuseFS::FuseHandle *) handle;

    if (ioctl(fh->fd, FS_IOC_GETVERSION, &igen)) {
        TRACE(Trace::error, errno);
        THROW(errno, errno, fh->fusepath);
    }

    return igen;
}

unsigned long long FsObj::getINode()

{
    unsigned long long ino = 0;
    struct stat statbuf;
    FuseFS::FuseHandle *fh = (FuseFS::FuseHandle *) handle;

    memset(&statbuf, 0, sizeof(statbuf));
    if (fstat(fh->fd, &statbuf) == -1) {
        TRACE(Trace::error, errno);
        THROW(errno, errno, fh->fusepath);
    }

    ino = statbuf.st_ino;

    return ino;
}

std::string FsObj::getTapeId()

{
    return "";
}

void FsObj::lock()

{
    FuseFS::FuseHandle *fh = (FuseFS::FuseHandle *) handle;
    if (flock(fh->fd, LOCK_EX) == -1) {
        TRACE(Trace::error, errno);
        MSG(LTFSDMF0032E, fh->fd);
    }
}

void FsObj::unlock()

{
    FuseFS::FuseHandle *fh = (FuseFS::FuseHandle *) handle;
    if (flock(fh->fd, LOCK_UN) == -1) {
        TRACE(Trace::error, errno);
        MSG(LTFSDMF0033E, fh->fd);
    }
}

long FsObj::read(long offset, unsigned long size, char *buffer)

{
    long rsize = 0;
    FuseFS::FuseHandle *fh = (FuseFS::FuseHandle *) handle;
    rsize = ::read(fh->fd, buffer, size);

    if (rsize == -1) {
        TRACE(Trace::error, errno);
        THROW(errno, errno, fh->fusepath);
    }

    return rsize;
}

long FsObj::write(long offset, unsigned long size, char *buffer)

{
    long wsize = 0;
    FuseFS::FuseHandle *fh = (FuseFS::FuseHandle *) handle;
    wsize = ::write(fh->fd, buffer, size);

    if (wsize == -1) {
        TRACE(Trace::error, errno);
        THROW(errno, errno, fh->fusepath);
    }

    return wsize;
}

void FsObj::addAttribute(mig_attr_t value)

{
    FuseFS::FuseHandle *fh = (FuseFS::FuseHandle *) handle;

    if (fsetxattr(fh->fd, Const::OPEN_LTFS_EA_MIGINFO_EXT.c_str(),
            (void *) &value, sizeof(value), 0) == -1) {
        TRACE(Trace::error, errno);
        THROW(errno, errno, fh->fusepath);
    }
}

void FsObj::remAttribute()

{
    FuseFS::FuseHandle *fh = (FuseFS::FuseHandle *) handle;

    if (fremovexattr(fh->fd, Const::OPEN_LTFS_EA_MIGINFO_EXT.c_str()) == -1) {
        TRACE(Trace::error, errno);
        MSG(LTFSDMF0018W, Const::OPEN_LTFS_EA_MIGINFO_EXT);
        if ( errno != ENODATA)
            THROW(errno, errno, fh->fusepath);
    }

    if (fremovexattr(fh->fd, Const::OPEN_LTFS_EA_MIGINFO_INT.c_str()) == -1) {
        TRACE(Trace::error, errno);
        MSG(LTFSDMF0018W, Const::OPEN_LTFS_EA_MIGINFO_INT);
        if ( errno != ENODATA)
            THROW(errno, errno, fh->fusepath);
    }
}

FsObj::mig_attr_t FsObj::getAttribute()

{
    FuseFS::FuseHandle *fh = (FuseFS::FuseHandle *) handle;
    FsObj::mig_attr_t value;
    memset(&value, 0, sizeof(mig_attr_t));

    if (fgetxattr(fh->fd, Const::OPEN_LTFS_EA_MIGINFO_EXT.c_str(),
            (void *) &value, sizeof(value)) == -1) {
        if ( errno != ENODATA) {
            TRACE(Trace::error, errno);
            THROW(errno, errno, fh->fusepath);
        }
    }

    return value;
}

void FsObj::preparePremigration()

{
    FuseFS::FuseHandle *fh = (FuseFS::FuseHandle *) handle;

    FuseFS::setMigInfoAt(fh->fd, FuseFS::mig_info::state_num::IN_MIGRATION);
}

void FsObj::finishPremigration()

{
    FuseFS::FuseHandle *fh = (FuseFS::FuseHandle *) handle;

    FuseFS::setMigInfoAt(fh->fd, FuseFS::mig_info::state_num::PREMIGRATED);
}

void FsObj::prepareRecall()

{
    FuseFS::FuseHandle *fh = (FuseFS::FuseHandle *) handle;

    FuseFS::setMigInfoAt(fh->fd, FuseFS::mig_info::state_num::IN_RECALL);
}

void FsObj::finishRecall(FsObj::file_state fstate)

{
    FuseFS::mig_info miginfo;
    FuseFS::FuseHandle *fh = (FuseFS::FuseHandle *) handle;

    if (fstate == FsObj::PREMIGRATED) {
        FuseFS::setMigInfoAt(fh->fd, FuseFS::mig_info::state_num::PREMIGRATED);
    } else {
        miginfo = FuseFS::getMigInfoAt(fh->fd);
        const timespec timestamp[2] = { miginfo.statinfo.st_atim,
                miginfo.statinfo.st_mtim };

        if (futimens(fh->fd, timestamp) == -1)
            MSG(LTFSDMF0017E, fh->fusepath);

        FuseFS::setMigInfoAt(fh->fd, FuseFS::mig_info::state_num::NO_STATE);
    }
}

void FsObj::prepareStubbing()

{
    FuseFS::FuseHandle *fh = (FuseFS::FuseHandle *) handle;

    FuseFS::setMigInfoAt(fh->fd, FuseFS::mig_info::state_num::STUBBING);
}

void FsObj::stub()

{
    FuseFS::FuseHandle *fh = (FuseFS::FuseHandle *) handle;
    struct stat statbuf;

    if (fstat(fh->fd, &statbuf) == -1) {
        TRACE(Trace::error, errno);
        THROW(errno, errno, fh->fusepath);
    }

    if (ftruncate(fh->fd, 0) == -1) {
        TRACE(Trace::error, errno);
        MSG(LTFSDMF0016E, fh->fusepath);
        FuseFS::setMigInfoAt(fh->fd, FuseFS::mig_info::state_num::PREMIGRATED);
        THROW(errno, errno, fh->fusepath);
    }

    FuseFS::setMigInfoAt(fh->fd, FuseFS::mig_info::state_num::MIGRATED);
}

FsObj::file_state FsObj::getMigState()
{
    FuseFS::FuseHandle *fh = (FuseFS::FuseHandle *) handle;
    FsObj::file_state state = FsObj::RESIDENT;
    FuseFS::mig_info miginfo;

    miginfo = FuseFS::getMigInfoAt(fh->fd);

    switch (miginfo.state) {
        case FuseFS::mig_info::state_num::NO_STATE:
        case FuseFS::mig_info::state_num::IN_MIGRATION:
            state = FsObj::RESIDENT;
            break;
        case FuseFS::mig_info::state_num::MIGRATED:
        case FuseFS::mig_info::state_num::IN_RECALL:
            state = FsObj::MIGRATED;
            break;
        case FuseFS::mig_info::state_num::PREMIGRATED:
        case FuseFS::mig_info::state_num::STUBBING:
            state = FsObj::PREMIGRATED;
    }

    return state;
}
