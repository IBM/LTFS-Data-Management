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
std::vector<FuseFS*> managedFss;
std::unique_lock<std::mutex> *trecall_lock;
}
;

LTFSDmCommServer recrequest(Const::RECALL_SOCKET_FILE);

Connector::Connector(bool cleanup_) :
        cleanup(cleanup_)

{
    clock_gettime(CLOCK_REALTIME, &starttime);
}

Connector::~Connector()

{
    try {
        std::string mountpt;

        if (cleanup)
            MSG(LTFSDMF0025I);

        for (auto const& fn : FuseConnector::managedFss) {
            mountpt = fn->getMountPoint();
            delete (fn);
            if (rmdir(mountpt.c_str()) == -1)
                MSG(LTFSDMF0008W, mountpt.c_str());
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
        throw(EXCEPTION(Const::UNSET));
    }
}

void Connector::endTransRecalls()

{
    recrequest.closeRef();
}

Connector::rec_info_t Connector::getEvents()

{
    Connector::rec_info_t recinfo;

    recrequest.accept();

    try {
        recrequest.recv();
    } catch (const std::exception& e) {
        MSG(LTFSDMF0019E, e.what(), errno);
        throw(EXCEPTION(Error::LTFSDM_GENERAL_ERROR));
    }

    const LTFSDmProtocol::LTFSDmTransRecRequest request = recrequest.transrecrequest();

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
    LTFSDmProtocol::LTFSDmTransRecResp *trecresp = recinfo.conn_info->reqrequest->mutable_transrecresp();

    trecresp->set_success(success);

    try {
        recinfo.conn_info->reqrequest->send();
    } catch (const std::exception& e) {
        TRACE(Trace::error, e.what());
        MSG(LTFSDMS0007E);
    }

    TRACE(Trace::always, recinfo.filename, success);

    recinfo.conn_info->reqrequest->closeAcc();

    delete(recinfo.conn_info->reqrequest);
    delete(recinfo.conn_info);
}

void Connector::terminate()

{
    Connector::connectorTerminate = true;

    LTFSDmCommClient commCommand(Const::RECALL_SOCKET_FILE);

    try {
        commCommand.connect();
    } catch (const std::exception& e) {
        MSG(LTFSDMF0020E, e.what(), errno);
        throw(EXCEPTION(Error::LTFSDM_GENERAL_ERROR));
    }

    LTFSDmProtocol::LTFSDmTransRecRequest *recrequest = commCommand.mutable_transrecrequest();

    recrequest->set_toresident(false);
    recrequest->set_fsid(0);
    recrequest->set_igen(0);
    recrequest->set_ino(0);
    recrequest->set_filename("");

    try {
        commCommand.send();
    } catch (const std::exception& e) {
        MSG(LTFSDMF0024E);
        throw(EXCEPTION(errno, errno));
    }
}

struct FuseHandle
{
    std::string fileName;
    std::string sourcePath;
    int fd;
};

FsObj::FsObj(std::string fileName) :
        handle(NULL), handleLength(0), isLocked(false), handleFree(true)

{
    FuseHandle *fh = new FuseHandle();
    char source_path[PATH_MAX];

    fh->fileName = fileName;

    memset(source_path, 0, sizeof(source_path));
    if (getxattr(fileName.c_str(), Const::OPEN_LTFS_EA_FSINFO.c_str(),
            source_path, sizeof(source_path) - 1) == -1) {
        if ( errno != ENODATA)
            TRACE(Trace::error, errno);
        fh->fd = -1;
    } else {
        fh->fd = open(source_path, O_RDWR);
        if (fh->fd == -1)
            TRACE(Trace::error, errno);
    }

    fh->sourcePath = source_path;

    handle = (void *) fh;
    handleLength = fileName.size();
}

FsObj::FsObj(Connector::rec_info_t recinfo) :
        handle(NULL), handleLength(0), isLocked(false), handleFree(true)

{
    FuseHandle *fh = new FuseHandle();

    fh->sourcePath = recinfo.filename;
    fh->fd = open(recinfo.filename.c_str(), O_RDWR);

    if (fh->fd == -1) {
        TRACE(Trace::error, errno);
        throw(EXCEPTION(errno, errno, recinfo.filename));
    }

    handle = (void *) fh;
    handleLength = recinfo.filename.size();
}

FsObj::~FsObj()

{
    try {
        FuseHandle *fh = (FuseHandle *) handle;

        if (fh->fd != -1)
            close(fh->fd);

        delete (fh);
    } catch (...) {
        kill(getpid(), SIGTERM);
    }
}

bool FsObj::isFsManaged()

{
    int val;
    FuseHandle *fh = (FuseHandle *) handle;
    std::unique_lock<std::mutex> lock(FuseConnector::mtx);

    if (getxattr(fh->fileName.c_str(), Const::OPEN_LTFS_EA_MANAGED.c_str(),
            &val, sizeof(val)) == -1) {
        if ( errno == ENODATA)
            return false;
        return true;
    }

    return (val == 1);
}

void FsObj::manageFs(bool setDispo, struct timespec starttime,
        std::string mountPoint, std::string fsName)

{
    int managed = 1;
    FuseHandle *fh = (FuseHandle *) handle;
    char mountpt[PATH_MAX];
    char fsname[PATH_MAX];
    struct stat statbuf;
    FuseFS *FS;

    if (mountPoint.compare("") == 0) {
        if (getxattr(fh->fileName.c_str(), Const::OPEN_LTFS_EA_MOUNTPT.c_str(),
                &mountpt, sizeof(mountpt)) == -1) {
            if ( errno != ENODATA)
                TRACE(Trace::error, errno);
            mountPoint = fh->fileName + ".managed";
        } else {
            mountPoint = mountpt;
        }
    }

    if (fsName.compare("") == 0) {
        if (getxattr(fh->fileName.c_str(), Const::OPEN_LTFS_EA_FSNAME.c_str(),
                &fsname, sizeof(fsname)) == -1) {
            if ( errno != ENODATA)
                TRACE(Trace::error, errno);
            fsName = std::string("[") + fh->fileName + "]";
        } else {
            fsName = fsname;
        }
    } else {
        fsName = std::string("[") + fsName + "]";
    }

    umount2(mountPoint.c_str(), MNT_FORCE);

    if (::stat(mountPoint.c_str(), &statbuf) == 0) {
        if (rmdir(mountPoint.c_str()) == -1) {
            TRACE(Trace::error, errno);
            MSG(LTFSDMF0012E, mountPoint, fh->fileName);
            managed = 0;
        }
    }

    if (managed == 1) {
        if (mkdir(mountPoint.c_str(), 700) == -1) {
            TRACE(Trace::error, errno);
            MSG(LTFSDMF0012E, mountPoint, fh->fileName);
            managed = 0;
        }
    }

    if (managed == 1) {
        try {
            FS = new FuseFS(fh->fileName, mountPoint, fsName, starttime);
        } catch (const std::exception& e) {
            TRACE(Trace::error, e.what());
            managed = 0;
        }
    }

    std::unique_lock<std::mutex> lock(FuseConnector::mtx);

    if (managed) {
        if (setxattr(fh->fileName.c_str(), Const::OPEN_LTFS_EA_MOUNTPT.c_str(),
                mountPoint.c_str(), mountPoint.size() + 1, 0) == -1) {
            TRACE(Trace::error, errno);
            MSG(LTFSDMF0009E, fh->fileName.c_str());
            if (managed == 1)
                delete (FS);
            throw(EXCEPTION(Error::LTFSDM_FS_ADD_ERROR, errno, fh->fileName));
        } else if (setxattr(fh->fileName.c_str(),
                Const::OPEN_LTFS_EA_FSNAME.c_str(), fsName.c_str(),
                fsName.size() + 1, 0) == -1) {
            TRACE(Trace::error, errno);
            MSG(LTFSDMF0009E, fh->fileName.c_str());
            if (managed == 1)
                delete (FS);
            throw(EXCEPTION(Error::LTFSDM_FS_ADD_ERROR, errno, fh->fileName));
        } else if (setxattr(fh->fileName.c_str(),
                Const::OPEN_LTFS_EA_MANAGED.c_str(), &managed, sizeof(managed),
                0) == -1) {
            TRACE(Trace::error, errno);
            MSG(LTFSDMF0009E, fh->fileName.c_str());
            if (managed == 1)
                delete (FS);
            throw(EXCEPTION(Error::LTFSDM_FS_ADD_ERROR, errno, fh->fileName));
        }
    }

    if (managed == 0) {
        rmdir(mountPoint.c_str());
        MSG(LTFSDMF0009E, fh->fileName.c_str());
        throw(EXCEPTION(Error::LTFSDM_FS_ADD_ERROR, fh->fileName));
    } else {
        FuseConnector::managedFss.push_back(FS);
    }
}

struct stat FsObj::stat()

{
    struct stat statbuf;
    FuseFS::mig_info miginfo;

    FuseHandle *fh = (FuseHandle *) handle;

    miginfo = FuseFS::getMigInfo(fh->sourcePath.c_str());

    if (miginfo.state != FuseFS::mig_info::state_num::NO_STATE
            && miginfo.state != FuseFS::mig_info::state_num::IN_MIGRATION) {
        statbuf = miginfo.statinfo;
    } else {
        memset(&statbuf, 0, sizeof(statbuf));
        if (fstat(fh->fd, &statbuf) == -1) {
            TRACE(Trace::error, errno);
            throw(EXCEPTION(errno, errno, fh->sourcePath));
        }
    }

    return statbuf;
}

unsigned long long FsObj::getFsId()

{
    unsigned long long fsid = 0;
    struct stat statbuf;
    FuseHandle *fh = (FuseHandle *) handle;

    memset(&statbuf, 0, sizeof(statbuf));
    if (fstat(fh->fd, &statbuf) == -1) {
        TRACE(Trace::error, errno);
        throw(EXCEPTION(errno, errno, fh->sourcePath));
    }

    // TODO
    fsid = statbuf.st_dev;

    return fsid;
}

unsigned int FsObj::getIGen()

{
    unsigned int igen = 0;
    FuseHandle *fh = (FuseHandle *) handle;

    if (ioctl(fh->fd, FS_IOC_GETVERSION, &igen)) {
        TRACE(Trace::error, errno);
        throw(EXCEPTION(errno, errno, fh->sourcePath));
    }

    return igen;
}

unsigned long long FsObj::getINode()

{
    unsigned long long ino = 0;
    struct stat statbuf;
    FuseHandle *fh = (FuseHandle *) handle;

    memset(&statbuf, 0, sizeof(statbuf));
    if (fstat(fh->fd, &statbuf) == -1) {
        TRACE(Trace::error, errno);
        throw(EXCEPTION(errno, errno, fh->sourcePath));
    }

    ino = statbuf.st_ino;

    return ino;
}

std::string FsObj::getTapeId()

{
    std::stringstream sstr;

    sstr << "DV148" << random() % 2 << "L6";

    return sstr.str();
}

void FsObj::lock()

{
}

void FsObj::unlock()

{
}

long FsObj::read(long offset, unsigned long size, char *buffer)

{
    long rsize = 0;
    FuseHandle *fh = (FuseHandle *) handle;
    rsize = ::read(fh->fd, buffer, size);

    if (rsize == -1) {
        TRACE(Trace::error, errno);
        throw(EXCEPTION(errno, errno, fh->sourcePath));
    }

    return rsize;
}

long FsObj::write(long offset, unsigned long size, char *buffer)

{
    long wsize = 0;
    FuseHandle *fh = (FuseHandle *) handle;
    wsize = ::write(fh->fd, buffer, size);

    if (wsize == -1) {
        TRACE(Trace::error, errno);
        throw(EXCEPTION(errno, errno, fh->sourcePath));
    }

    return wsize;
}

void FsObj::addAttribute(mig_attr_t value)

{
    FuseHandle *fh = (FuseHandle *) handle;

    if (fsetxattr(fh->fd, Const::OPEN_LTFS_EA_MIGINFO_EXT.c_str(),
            (void *) &value, sizeof(value), 0) == -1) {
        TRACE(Trace::error, errno);
        throw(EXCEPTION(errno, errno, fh->sourcePath));
    }
}

void FsObj::remAttribute()

{
    FuseHandle *fh = (FuseHandle *) handle;

    if (fremovexattr(fh->fd, Const::OPEN_LTFS_EA_MIGINFO_EXT.c_str()) == -1) {
        TRACE(Trace::error, errno);
        MSG(LTFSDMF0018W, Const::OPEN_LTFS_EA_MIGINFO_EXT);
        if ( errno != ENODATA)
            throw(EXCEPTION(errno, errno, fh->sourcePath));
    }

    if (fremovexattr(fh->fd, Const::OPEN_LTFS_EA_MIGINFO_INT.c_str()) == -1) {
        TRACE(Trace::error, errno);
        MSG(LTFSDMF0018W, Const::OPEN_LTFS_EA_MIGINFO_INT);
        if ( errno != ENODATA)
            throw(EXCEPTION(errno, errno, fh->sourcePath));
    }
}

FsObj::mig_attr_t FsObj::getAttribute()

{
    FuseHandle *fh = (FuseHandle *) handle;
    FsObj::mig_attr_t value;
    memset(&value, 0, sizeof(mig_attr_t));

    if (fgetxattr(fh->fd, Const::OPEN_LTFS_EA_MIGINFO_EXT.c_str(),
            (void *) &value, sizeof(value)) == -1) {
        if ( errno != ENODATA) {
            TRACE(Trace::error, errno);
            throw(EXCEPTION(errno, errno, fh->sourcePath));
        }
    }

    return value;
}

void FsObj::preparePremigration()

{
    FuseHandle *fh = (FuseHandle *) handle;

    FuseFS::setMigInfo(fh->sourcePath.c_str(),
            FuseFS::mig_info::state_num::IN_MIGRATION);
}

void FsObj::finishPremigration()

{
    FuseHandle *fh = (FuseHandle *) handle;

    FuseFS::setMigInfo(fh->sourcePath.c_str(),
            FuseFS::mig_info::state_num::PREMIGRATED);
}

void FsObj::prepareRecall()

{
    FuseHandle *fh = (FuseHandle *) handle;

    FuseFS::setMigInfo(fh->sourcePath.c_str(),
            FuseFS::mig_info::state_num::IN_RECALL);
}

void FsObj::finishRecall(FsObj::file_state fstate)

{
    FuseFS::mig_info miginfo;
    FuseHandle *fh = (FuseHandle *) handle;

    if (fstate == FsObj::PREMIGRATED) {
        FuseFS::setMigInfo(fh->sourcePath.c_str(),
                FuseFS::mig_info::state_num::PREMIGRATED);
    } else {
        miginfo = FuseFS::getMigInfo(fh->sourcePath.c_str());
        const timespec timestamp[2] = { miginfo.statinfo.st_atim,
                miginfo.statinfo.st_mtim };

        if (futimens(fh->fd, timestamp) == -1)
            MSG(LTFSDMF0017E, fh->fileName);

        FuseFS::setMigInfo(fh->sourcePath.c_str(),
                FuseFS::mig_info::state_num::NO_STATE);
    }
}

void FsObj::prepareStubbing()

{
    FuseHandle *fh = (FuseHandle *) handle;

    FuseFS::setMigInfo(fh->sourcePath.c_str(),
            FuseFS::mig_info::state_num::STUBBING);
}

void FsObj::stub()

{
    FuseHandle *fh = (FuseHandle *) handle;
    struct stat statbuf;

    if (fstat(fh->fd, &statbuf) == -1) {
        TRACE(Trace::error, errno);
        throw(EXCEPTION(errno, errno, fh->sourcePath));
    }

    if (ftruncate(fh->fd, 0) == -1) {
        TRACE(Trace::error, errno);
        MSG(LTFSDMF0016E, fh->fileName);
        FuseFS::setMigInfo(fh->sourcePath.c_str(),
                FuseFS::mig_info::state_num::PREMIGRATED);
        throw(EXCEPTION(errno, errno, fh->sourcePath));
    }

    FuseFS::setMigInfo(fh->sourcePath.c_str(),
            FuseFS::mig_info::state_num::MIGRATED);
}

FsObj::file_state FsObj::getMigState()
{
    FuseHandle *fh = (FuseHandle *) handle;
    FsObj::file_state state = FsObj::RESIDENT;
    FuseFS::mig_info miginfo;

    miginfo = FuseFS::getMigInfo(fh->sourcePath.c_str());

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
