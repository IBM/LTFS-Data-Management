#include <stdio.h> // for rename
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/mount.h>
#include <sys/file.h>
#include <linux/fs.h>
#include <dirent.h>
#include <sys/time.h>
#include <sys/types.h>
#include <signal.h>
#include <sys/xattr.h>
#include <sys/ioctl.h>
#include <sys/resource.h>
#include <assert.h>
#include <errno.h>

#define FUSE_USE_VERSION 26

#include <fuse.h>

#include <string>
#include <sstream>
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
#include "ltfsdmd.ofs.h"

std::mutex FuseLock::mtx;

FuseLock::FuseLock(std::string identifier, FuseLock::lockType _type,
        FuseLock::lockOperation _operation) :
        id(identifier), fd(Const::UNSET), type(_type), operation(_operation)

{
    id += '.';
    id += type;

    std::lock_guard<std::mutex> lock(FuseLock::mtx);

    if ((fd = open(id.c_str(), O_RDONLY)) == -1) {
        TRACE(Trace::error, id, errno);
        THROW(errno, id, errno);
    }
}

FuseLock::~FuseLock()

{
    if (fd != Const::UNSET)
        close(fd);
}

void FuseLock::lock()

{
    if (flock(fd, (operation == FuseLock::lockshared ? LOCK_SH : LOCK_EX))
            == -1) {
        TRACE(Trace::error, id, errno);
        THROW(errno, id, errno);
    }
}

bool FuseLock::try_lock()

{
    if (flock(fd,
            (operation == FuseLock::lockshared ? LOCK_SH : LOCK_EX) | LOCK_NB)
            == -1) {
        if ( errno == EWOULDBLOCK)
            return false;
        TRACE(Trace::error, id, errno);
        THROW(errno, id, errno);
    }

    return true;
}

void FuseLock::unlock()

{
    if (flock(fd, LOCK_UN) == -1) {
        TRACE(Trace::error, id, errno);
        THROW(errno, id, errno);
    }
}

const char *FuseFS::relPath(const char *path)

{
    const char *rpath = path;

    if (std::string(path).compare("/") == 0)
        return ".";
    else if (path[0] == '/')
        rpath++;

    return rpath;

}

std::string FuseFS::lockPath(std::string path)

{
    std::stringstream lpath;
    struct stat statbuf;

    if (fstatat(getffs()->rootFd, FuseFS::relPath(path.c_str()), &statbuf,
    AT_SYMLINK_NOFOLLOW) == -1) {
        TRACE(Trace::error, path, errno);
        return "";
    }

    lpath << getffs()->mountpt << Const::OPEN_LTFS_LOCK_DIR << "/"
            << statbuf.st_ino;

    TRACE(Trace::always, lpath.str());

    return lpath.str();
}

FuseFS::mig_info FuseFS::genMigInfoAt(int fd, FuseFS::mig_info::state_num state)

{
    FuseFS::mig_info miginfo;

    memset(&miginfo, 0, sizeof(miginfo));

    if (fstat(fd, &miginfo.statinfo)) {
        TRACE(Trace::error, errno);
        THROW(errno, errno, fd);
    }

    miginfo.state = state;

    clock_gettime(CLOCK_REALTIME, &miginfo.changed);

    return miginfo;
}

FuseFS::mig_info FuseFS::genMigInfo(std::string fileName,
        FuseFS::mig_info::state_num state)

{
    FuseFS::mig_info miginfo;

    memset(&miginfo, 0, sizeof(miginfo));

    if (stat(fileName.c_str(), &miginfo.statinfo)) {
        TRACE(Trace::error, errno);
        THROW(errno, errno, fileName);
    }

    miginfo.state = state;

    clock_gettime(CLOCK_REALTIME, &miginfo.changed);

    return miginfo;
}

void FuseFS::setMigInfoAt(int fd, FuseFS::mig_info::state_num state)

{
    ssize_t size;
    FuseFS::mig_info miginfo_new;
    FuseFS::mig_info miginfo;

    TRACE(Trace::full, fd, state);

    memset(&miginfo, 0, sizeof(miginfo));

    miginfo_new = genMigInfoAt(fd, state);

    if ((size = fgetxattr(fd, Const::OPEN_LTFS_EA_MIGINFO_INT.c_str(),
            (void *) &miginfo, sizeof(miginfo))) == -1) {
        if ( errno != ENODATA) {
            THROW(errno, errno, fd);
        }
    } else if (size != sizeof(miginfo)) {
        THROW(EIO, size, sizeof(miginfo), fd);
    }

    if (miginfo.state != FuseFS::mig_info::state_num::NO_STATE) {
        miginfo_new.statinfo.st_size = miginfo.statinfo.st_size;
        miginfo_new.statinfo.st_atim = miginfo.statinfo.st_atim;
        miginfo_new.statinfo.st_mtim = miginfo.statinfo.st_mtim;
    }

    if (fsetxattr(fd, Const::OPEN_LTFS_EA_MIGINFO_INT.c_str(),
            (void *) &miginfo_new, sizeof(miginfo_new), 0) == -1) {
        THROW(errno, errno, fd);
    }
}

void FuseFS::setMigInfo(std::string fileName, FuseFS::mig_info::state_num state)

{
    ssize_t size;
    FuseFS::mig_info miginfo_new;
    FuseFS::mig_info miginfo;

    TRACE(Trace::full, fileName, state);

    memset(&miginfo, 0, sizeof(miginfo));

    miginfo_new = genMigInfo(fileName, state);

    if ((size = getxattr(fileName.c_str(),
            Const::OPEN_LTFS_EA_MIGINFO_INT.c_str(), (void *) &miginfo,
            sizeof(miginfo))) == -1) {
        if ( errno != ENODATA) {
            THROW(errno, errno, fileName);
        }
    } else if (size != sizeof(miginfo)) {
        THROW(EIO, size, sizeof(miginfo), fileName);
    }

    if (miginfo.state != FuseFS::mig_info::state_num::NO_STATE) {
        miginfo_new.statinfo.st_size = miginfo.statinfo.st_size;
        miginfo_new.statinfo.st_atim = miginfo.statinfo.st_atim;
        miginfo_new.statinfo.st_mtim = miginfo.statinfo.st_mtim;
    }

    if (setxattr(fileName.c_str(), Const::OPEN_LTFS_EA_MIGINFO_INT.c_str(),
            (void *) &miginfo_new, sizeof(miginfo_new), 0) == -1) {
        THROW(errno, errno, fileName);
    }
}
int FuseFS::remMigInfoAt(int fd)

{
    TRACE(Trace::full, fd);

    if (fremovexattr(fd, Const::OPEN_LTFS_EA_MIGINFO_INT.c_str()) == -1) {
        TRACE(Trace::error, errno);
        return errno;
    }

    if (fremovexattr(fd, Const::OPEN_LTFS_EA_MIGINFO_EXT.c_str()) == -1) {
        TRACE(Trace::error, errno);
        return errno;
    }

    return 0;
}

FuseFS::mig_info FuseFS::getMigInfoAt(int fd)

{
    ssize_t size;
    FuseFS::mig_info miginfo;

    memset(&miginfo, 0, sizeof(miginfo));

    if ((size = fgetxattr(fd, Const::OPEN_LTFS_EA_MIGINFO_INT.c_str(),
            (void *) &miginfo, sizeof(miginfo))) == -1) {
        // TODO
        /* check for errno */
        return miginfo;
    }

    if (size != sizeof(miginfo))
        THROW(EIO, size, sizeof(miginfo), fd);

    return miginfo;
}

bool FuseFS::needsRecovery(FuseFS::mig_info miginfo)

{
    if ((miginfo.state == FuseFS::mig_info::state_num::IN_MIGRATION)
            | (miginfo.state == FuseFS::mig_info::state_num::STUBBING)
            | (miginfo.state == FuseFS::mig_info::state_num::IN_RECALL)) {

        if (getffs()->starttime.tv_sec < miginfo.changed.tv_sec)
            return false;
        else if ((getffs()->starttime.tv_sec == miginfo.changed.tv_sec)
                && (getffs()->starttime.tv_nsec < miginfo.changed.tv_nsec))
            return false;
        else
            return true;
    }

    return false;
}

void FuseFS::recoverState(const char *path, FuseFS::mig_info::state_num state)

{
    int fd;

    if ((fd = openat(getffs()->rootFd, FuseFS::relPath(path), O_RDWR)) == -1) {
        TRACE(Trace::error, errno);
        MSG(LTFSDMF0010E, path);
        return;
    }

    std::string fusepath = getffs()->mountpt + std::string(path);

    TRACE(Trace::error, fusepath, state);

    switch (state) {
        case FuseFS::mig_info::state_num::IN_MIGRATION:
            MSG(LTFSDMF0013W, fusepath);
            if (fremovexattr(fd, Const::OPEN_LTFS_EA_MIGINFO_EXT.c_str())
                    == -1) {
                TRACE(Trace::error, errno);
                if ( errno != ENODATA)
                    MSG(LTFSDMF0018W, Const::OPEN_LTFS_EA_MIGINFO_EXT);
            }
            if (fremovexattr(fd, Const::OPEN_LTFS_EA_MIGINFO_INT.c_str())
                    == -1) {
                TRACE(Trace::error, errno);
                if ( errno != ENODATA)
                    MSG(LTFSDMF0018W, Const::OPEN_LTFS_EA_MIGINFO_INT);
            }
            break;
        case FuseFS::mig_info::state_num::STUBBING:
            MSG(LTFSDMF0014W, fusepath);
            FuseFS::setMigInfoAt(fd, FuseFS::mig_info::state_num::MIGRATED);
            if (ftruncate(fd, 0) == -1) {
                TRACE(Trace::error, errno);
                MSG(LTFSDMF0016E, fusepath);
                FuseFS::setMigInfoAt(fd,
                        FuseFS::mig_info::state_num::PREMIGRATED);
            }
            break;
        case FuseFS::mig_info::state_num::IN_RECALL:
            MSG(LTFSDMF0015W, fusepath);
            FuseFS::setMigInfoAt(fd, FuseFS::mig_info::state_num::MIGRATED);
            if (ftruncate(fd, 0) == -1) {
                TRACE(Trace::error, errno);
                MSG(LTFSDMF0016E, fusepath);
            }
            break;
        default:
            assert(0);
    }

    close(fd);
}

int FuseFS::recall_file(FuseFS::ltfsdm_file_info *linfo, bool toresident)

{
    struct stat statbuf;
    unsigned int igen;
    bool success;
    std::string path;
    struct fuse_context *fc = fuse_get_context();

    if (fstat(linfo->fd, &statbuf) == -1) {
        TRACE(Trace::error, fc->pid, errno);
        return (-1 * errno);
    }

    if (ioctl(linfo->fd, FS_IOC_GETVERSION, &igen)) {
        TRACE(Trace::error, fc->pid, errno);
        return (-1 * errno);
    }

    path = getffs()->mountpt;
    path.append(linfo->fusepath);
    TRACE(Trace::always, path, statbuf.st_ino, toresident, fc->pid);

    if (Connector::recallEventSystemStopped == true)
        return -1;

    LTFSDmCommClient recRequest(Const::RECALL_SOCKET_FILE);

    try {
        recRequest.connect();
    } catch (const std::exception& e) {
        MSG(LTFSDMF0021E, e.what(), errno);
        return -1;
    }

    LTFSDmProtocol::LTFSDmTransRecRequest *recrequest =
            recRequest.mutable_transrecrequest();

    recrequest->set_key(getffs()->ltfsdmKey);
    recrequest->set_toresident(toresident);
    recrequest->set_fsid(statbuf.st_dev);
    recrequest->set_igen(igen);
    recrequest->set_ino(statbuf.st_ino);
    recrequest->set_filename(path);

    try {
        recRequest.send();
    } catch (const std::exception& e) {
        MSG(LTFSDMF0024E);
        return -1;
    }

    try {
        recRequest.recv();
    } catch (const std::exception& e) {
        MSG(LTFSDMF0022E, e.what(), errno);
        return -1;
    }

    const LTFSDmProtocol::LTFSDmTransRecResp recresp =
            recRequest.transrecresp();

    success = recresp.success();

    TRACE(Trace::always, path, statbuf.st_ino, success);

    if (success == false)
        return -1;
    else
        return 0;
}

bool FuseFS::procIsOpenLTFS(pid_t tid)
{
    struct stat statbuf;

    pid_t pids[] = { getffs()->mainpid, getpid() };

    for (pid_t pid : pids) {
        std::stringstream spath;
        spath << "/proc/" << pid << "/task/" << tid;
        if (stat(spath.str().c_str(), &statbuf) == 0)
            return true;
    }

    return false;
}

int FuseFS::ltfsdm_getattr(const char *path, struct stat *statbuf)

{
    FuseFS::mig_info miginfo;
    struct fuse_context *fc = fuse_get_context();
    pid_t pid = fc->pid;
    int fd;

    memset(statbuf, 0, sizeof(struct stat));

    TRACE(Trace::always, pid, FuseFS::procIsOpenLTFS(pid), path);

    while (getffs()->rootFd == Const::UNSET
            && FuseFS::procIsOpenLTFS(pid) == false)
        sleep(1);

    if (getffs()->rootFd == Const::UNSET && FuseFS::procIsOpenLTFS(pid) == true) {
        if (Const::OPEN_LTFS_IOCTL.compare(path) == 0) {
            statbuf->st_mode = S_IFREG | S_IRWXU;
            goto end;
        }

        if (std::string("/").compare(path) == 0
                || Const::OPEN_LTFS_CACHE_DIR.compare(path) == 0
                || Const::OPEN_LTFS_CACHE_MP.compare(path) == 0) {
            statbuf->st_mode = S_IFDIR | S_IRWXU;
            goto end;
        }
    }

    if (std::string("/").compare(path) == 0) {
        if (fstatat(getffs()->rootFd, ".", statbuf, AT_SYMLINK_NOFOLLOW)
                == -1) {
            statbuf->st_mode = S_IFDIR | S_IRWXU;
            goto end;
        }
        goto end;
    }

    if (std::string(path).compare(0, Const::OPEN_LTFS_LOCK_DIR.size(),
            Const::OPEN_LTFS_LOCK_DIR) == 0) {
        if (std::string(path).size() == Const::OPEN_LTFS_LOCK_DIR.size()) {
            statbuf->st_mode = S_IFDIR | S_IRWXU;
            goto end;
        } else if (path[Const::OPEN_LTFS_LOCK_DIR.size()] == '/') {
            statbuf->st_mode = S_IFREG | S_IRWXU;
            statbuf->st_ino = strtoul(
                    path + (Const::OPEN_LTFS_LOCK_DIR.size() + 1), 0, 10);
            goto end;
        }
    }

    if (fstatat(getffs()->rootFd, FuseFS::relPath(path), statbuf,
    AT_SYMLINK_NOFOLLOW) == -1) {
        if (Const::OPEN_LTFS_IOCTL.compare(path) == 0) {
            return (-1 * EACCES);
        } else {
            return (-1 * errno);
        }
    } else {
        if ((fd = openat(getffs()->rootFd, FuseFS::relPath(path), O_RDONLY))
                == -1)
            goto end;
        miginfo = getMigInfoAt(fd);
        if (FuseFS::needsRecovery(miginfo) == true)
            FuseFS::recoverState(path, miginfo.state);
        close(fd);
        if (miginfo.state != FuseFS::mig_info::state_num::NO_STATE) {
            statbuf->st_size = miginfo.statinfo.st_size;
            statbuf->st_atim = miginfo.statinfo.st_atim;
            statbuf->st_mtim = miginfo.statinfo.st_mtim;
        }
        goto end;
    }

    end:
    TRACE(Trace::always, pid, path);
    return 0;
}

int FuseFS::ltfsdm_access(const char *path, int mask)

{
    if (faccessat(getffs()->rootFd, FuseFS::relPath(path), mask, 0) == -1)
        return (-1 * errno);
    else
        return 0;
}

int FuseFS::ltfsdm_readlink(const char *path, char *buffer, size_t size)

{
    memset(buffer, 0, size);

    if (readlinkat(getffs()->rootFd, FuseFS::relPath(path), buffer, size - 1)
            == -1)
        return (-1 * errno);
    else
        return 0;
}

int FuseFS::ltfsdm_opendir(const char *path, struct fuse_file_info *finfo)

{
    FuseFS::ltfsdm_dir_info *dirinfo = NULL;
    DIR *dir = NULL;
    int fd;

    if ((fd = openat(getffs()->rootFd, FuseFS::relPath(path), O_RDONLY)) == -1)
        return (-1 * errno);

    if ((dir = fdopendir(fd)) == 0) {
        close(fd);
        return (-1 * errno);
    }

    dirinfo = new (FuseFS::ltfsdm_dir_info);
    dirinfo->dir = dir;
    dirinfo->dentry = NULL;
    dirinfo->offset = 0;

    finfo->fh = (unsigned long) dirinfo;

    return 0;
}

int FuseFS::ltfsdm_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
        off_t offset, struct fuse_file_info *finfo)

{
    struct stat statbuf;
    FuseFS::mig_info miginfo;
    off_t next;
    FuseFS::ltfsdm_dir_info *dirinfo = (FuseFS::ltfsdm_dir_info *) finfo->fh;
    int fd;

    assert(path == NULL);

    if (dirinfo == NULL)
        return (-1 * EBADF);

    if (offset != dirinfo->offset) {
        seekdir(dirinfo->dir, offset);
        dirinfo->dentry = NULL;
        dirinfo->offset = offset;
    }

    while (true) {
        if (!dirinfo->dentry) {
            if ((dirinfo->dentry = readdir(dirinfo->dir)) == NULL) {
                break;
            }
        }

        if (fstatat(dirfd(dirinfo->dir), dirinfo->dentry->d_name, &statbuf,
        AT_SYMLINK_NOFOLLOW) == -1)
            return (-1 * errno);

        next = telldir(dirinfo->dir);

        if (S_ISREG(statbuf.st_mode)) {
            if ((fd = openat(dirfd(dirinfo->dir), dirinfo->dentry->d_name,
            O_RDONLY)) == -1)
                return (-1 * errno);
            miginfo = getMigInfoAt(fd);
            close(fd);
            if (miginfo.state != FuseFS::mig_info::state_num::NO_STATE
                    && miginfo.state
                            != FuseFS::mig_info::state_num::IN_MIGRATION)
                statbuf.st_size = miginfo.statinfo.st_size;
        }

        if (filler(buf, dirinfo->dentry->d_name, &statbuf, next))
            break;

        dirinfo->dentry = NULL;
        dirinfo->offset = next;
    }

    return 0;
}

int FuseFS::ltfsdm_releasedir(const char *path, struct fuse_file_info *finfo)

{
    FuseFS::ltfsdm_dir_info *dirinfo = (FuseFS::ltfsdm_dir_info *) finfo->fh;

    assert(path == NULL);

    if (dirinfo == NULL)
        return (-1 * EBADF);

    closedir(dirinfo->dir);
    delete (dirinfo);

    return 0;
}

int FuseFS::ltfsdm_mknod(const char *path, mode_t mode, dev_t rdev)

{
    struct fuse_context *fc = fuse_get_context();

    if (mknodat(getffs()->rootFd, FuseFS::relPath(path), mode, rdev) == -1) {
        return (-1 * errno);
    } else {
        fc = fuse_get_context();
        if (fchownat(getffs()->rootFd, FuseFS::relPath(path), fc->uid, fc->gid,
        AT_SYMLINK_NOFOLLOW) == -1)
            return (-1 * errno);
    }
    return 0;
}

int FuseFS::ltfsdm_mkdir(const char *path, mode_t mode)

{
    struct fuse_context *fc = fuse_get_context();

    if (mkdirat(getffs()->rootFd, FuseFS::relPath(path), mode) == -1) {
        return (-1 * errno);
    } else {
        fc = fuse_get_context();
        if (fchownat(getffs()->rootFd, FuseFS::relPath(path), fc->uid, fc->gid,
        AT_EMPTY_PATH | AT_SYMLINK_NOFOLLOW) == -1)
            return (-1 * errno);
    }
    return 0;
}

int FuseFS::ltfsdm_unlink(const char *path)

{
    if (unlinkat(getffs()->rootFd, FuseFS::relPath(path), 0) == -1)
        return (-1 * errno);
    else
        return 0;
}

int FuseFS::ltfsdm_rmdir(const char *path)

{
    if (unlinkat(getffs()->rootFd, FuseFS::relPath(path), AT_REMOVEDIR) == -1)
        return (-1 * errno);
    else
        return 0;
}

int FuseFS::ltfsdm_symlink(const char *target, const char *linkpath)

{
    if (symlinkat(target, getffs()->rootFd, FuseFS::relPath(linkpath)) == -1)
        return (-1 * errno);
    else
        return 0;
}

int FuseFS::ltfsdm_rename(const char *oldpath, const char *newpath)

{
    if (renameat(getffs()->rootFd, FuseFS::relPath(oldpath), getffs()->rootFd,
            FuseFS::relPath(newpath)) == -1)
        return (-1 * errno);
    else
        return 0;
}

int FuseFS::ltfsdm_link(const char *oldpath, const char *newpath)

{
    if (linkat(getffs()->rootFd, FuseFS::relPath(oldpath), getffs()->rootFd,
            FuseFS::relPath(newpath), AT_EMPTY_PATH) == -1)
        return (-1 * errno);
    else
        return 0;
}

int FuseFS::ltfsdm_chmod(const char *path, mode_t mode)

{
    if (fchmodat(getffs()->rootFd, FuseFS::relPath(path), mode, 0) == -1)
        return (-1 * errno);
    else
        return 0;
}

int FuseFS::ltfsdm_chown(const char *path, uid_t uid, gid_t gid)

{
    if (fchownat(getffs()->rootFd, FuseFS::relPath(path), uid, gid,
    AT_SYMLINK_NOFOLLOW) == -1)
        return (-1 * errno);
    else
        return 0;
}

int FuseFS::ltfsdm_truncate(const char *path, off_t size)

{
    FuseFS::mig_info migInfo;
    ssize_t attrsize;
    FuseFS::ltfsdm_file_info linfo = (FuseFS::ltfsdm_file_info ) { 0, 0, "" };

    linfo.fusepath = path;

    if ((linfo.fd = openat(getffs()->rootFd, FuseFS::relPath(path), O_WRONLY))
            == -1) {
        TRACE(Trace::error, errno);
        return (-1 * errno);
    }

    memset(&migInfo, 0, sizeof(FuseFS::mig_info));

    try {
        FuseLock main_lock(FuseFS::lockPath(path), FuseLock::main,
                FuseLock::lockshared);
        std::unique_lock<FuseLock> mainlock(main_lock);

        FuseLock trec_lock(FuseFS::lockPath(path), FuseLock::fuse,
                FuseLock::lockexclusive);

        std::lock_guard<FuseLock> treclock(trec_lock);

        if ((attrsize = fgetxattr(linfo.fd,
                Const::OPEN_LTFS_EA_MIGINFO_INT.c_str(), (void *) &migInfo,
                sizeof(migInfo))) == -1) {
            if ( errno != ENODATA) {
                close(linfo.fd);
                TRACE(Trace::error, fuse_get_context()->pid);
                return (-1 * errno);
            }
        }

        if ((size > 0)
                && ((migInfo.state == FuseFS::mig_info::state_num::MIGRATED)
                        || (migInfo.state
                                == FuseFS::mig_info::state_num::IN_RECALL))) {
            TRACE(Trace::full, path);
            mainlock.unlock();
            if (recall_file(&linfo, true) == -1) {
                close(linfo.fd);
                return (-1 * EIO);
            }
            mainlock.lock();
        } else if (attrsize != -1) {
            if (fremovexattr(linfo.fd, Const::OPEN_LTFS_EA_MIGINFO_INT.c_str())
                    == -1) {
                close(linfo.fd);
                return (-1 * EIO);
            }
            if (fremovexattr(linfo.fd, Const::OPEN_LTFS_EA_MIGINFO_EXT.c_str())
                    == -1) {
                close(linfo.fd);
                return (-1 * EIO);
            }
        }
    } catch (const std::exception& e) {
        TRACE(Trace::error, e.what());
        return (-1 * EIO);
    }

    if (ftruncate(linfo.fd, size) == -1) {
        close(linfo.fd);
        return (-1 * errno);
    }

    close(linfo.fd);

    return 0;
}

int FuseFS::ltfsdm_utimens(const char *path, const struct timespec times[2])

{
    if (utimensat(getffs()->rootFd, FuseFS::relPath(path), times,
    AT_SYMLINK_NOFOLLOW) == -1)
        return (-1 * errno);
    else
        return 0;
}

int FuseFS::ltfsdm_open(const char *path, struct fuse_file_info *finfo)

{
    int fd = Const::UNSET;
    int lfd = Const::UNSET;
    FuseFS::ltfsdm_file_info *linfo = NULL;

    if (getffs()->rootFd == Const::UNSET
            && Const::OPEN_LTFS_IOCTL.compare(path) == 0) {
        linfo = new (FuseFS::ltfsdm_file_info);
        linfo->fd = Const::UNSET;
        linfo->lfd = Const::UNSET;
        linfo->fusepath = path;
        linfo->main_lock = nullptr;
        finfo->fh = (unsigned long) linfo;
        return 0;
    }

    if (std::string(path).compare(0, Const::OPEN_LTFS_LOCK_DIR.size() + 1,
            Const::OPEN_LTFS_LOCK_DIR + "/") == 0) {
        TRACE(Trace::always, path);
        linfo = new (FuseFS::ltfsdm_file_info);
        linfo->fd = Const::UNSET;
        linfo->lfd = Const::UNSET;
        linfo->fusepath = path;
        linfo->main_lock = nullptr;
        finfo->fh = (unsigned long) linfo;
        return 0;
    }

    if ((fd = openat(getffs()->rootFd, FuseFS::relPath(path), finfo->flags))
            == -1) {
        TRACE(Trace::error, fuse_get_context()->pid);
        return (-1 * errno);
    }

    linfo = new (FuseFS::ltfsdm_file_info);

    linfo->fd = fd;
    linfo->lfd = lfd;
    linfo->fusepath = path;
    linfo->main_lock = nullptr;
    try {
        linfo->main_lock = new FuseLock(FuseFS::lockPath(path), FuseLock::main,
                FuseLock::lockshared);
    } catch (const std::exception& e) {
        TRACE(Trace::error, FuseFS::lockPath(path));
        return (-1 * EACCES);
    }

    finfo->fh = (unsigned long) linfo;

    return 0;
}

int FuseFS::ltfsdm_ftruncate(const char *path, off_t size,
        struct fuse_file_info *finfo)

{
    FuseFS::mig_info migInfo;
    ssize_t attrsize;
    FuseFS::ltfsdm_file_info *linfo = (FuseFS::ltfsdm_file_info *) finfo->fh;

    if (linfo == NULL) {
        TRACE(Trace::error, fuse_get_context()->pid);
        return (-1 * EBADF);
    }

    if (FuseFS::procIsOpenLTFS(fuse_get_context()->pid) == false) {
        memset(&migInfo, 0, sizeof(FuseFS::mig_info));

        std::unique_lock<FuseLock> mainlock(*(linfo->main_lock));

        try {
            FuseLock trec_lock(FuseFS::lockPath(linfo->fusepath),
                    FuseLock::fuse, FuseLock::lockexclusive);

            std::lock_guard<FuseLock> treclock(trec_lock);

            if ((attrsize = fgetxattr(linfo->fd,
                    Const::OPEN_LTFS_EA_MIGINFO_INT.c_str(), (void *) &migInfo,
                    sizeof(migInfo))) == -1) {
                if ( errno != ENODATA) {
                    TRACE(Trace::error, fuse_get_context()->pid);
                    return (-1 * errno);
                }
            }

            if ((size > 0)
                    && ((migInfo.state == FuseFS::mig_info::state_num::MIGRATED)
                            || (migInfo.state
                                    == FuseFS::mig_info::state_num::IN_RECALL))) {
                TRACE(Trace::full, linfo->fd);
                mainlock.unlock();
                if (recall_file(linfo, true) == -1) {
                    return (-1 * EIO);
                }
                mainlock.lock();
            } else if (attrsize != -1) {
                if (fremovexattr(linfo->fd,
                        Const::OPEN_LTFS_EA_MIGINFO_INT.c_str()) == -1)
                    return (-1 * EIO);
                if (fremovexattr(linfo->fd,
                        Const::OPEN_LTFS_EA_MIGINFO_EXT.c_str()) == -1)
                    return (-1 * EIO);
            }
        } catch (const std::exception& e) {
            TRACE(Trace::error, e.what());
            return (-1 * EIO);
        }
    }

    if (ftruncate(linfo->fd, size) == -1) {
        return (-1 * errno);
    }

    return 0;
}

int FuseFS::ltfsdm_read_buf(const char *path, struct fuse_bufvec **bufferp,
        size_t size, off_t offset, struct fuse_file_info *finfo)

{
    struct fuse_bufvec *source;
    FuseFS::mig_info migInfo;
    ssize_t attrsize;
    FuseFS::ltfsdm_file_info *linfo = (FuseFS::ltfsdm_file_info *) finfo->fh;

    assert(path == NULL);

    if (linfo == NULL) {
        TRACE(Trace::error, fuse_get_context()->pid);
        return (-1 * EBADF);
    }

    memset(&migInfo, 0, sizeof(FuseFS::mig_info));

    std::unique_lock<FuseLock> mainlock(*(linfo->main_lock));

    TRACE(Trace::always, linfo->fd);

    try {
        FuseLock trec_lock(FuseFS::lockPath(linfo->fusepath), FuseLock::fuse,
                FuseLock::lockexclusive);

        std::lock_guard<FuseLock> treclock(trec_lock);

        if ((attrsize = fgetxattr(linfo->fd,
                Const::OPEN_LTFS_EA_MIGINFO_INT.c_str(), (void *) &migInfo,
                sizeof(migInfo))) == -1) {
            if ( errno != ENODATA) {
                TRACE(Trace::error, fuse_get_context()->pid, errno);
                return (-1 * errno);
            }
        }

        if (migInfo.state == FuseFS::mig_info::state_num::MIGRATED
                || migInfo.state == FuseFS::mig_info::state_num::IN_RECALL) {
            TRACE(Trace::full, linfo->fd);
            mainlock.unlock();
            if (recall_file(linfo, false) == -1) {
                *bufferp = NULL;
                return (-1 * EIO);
            }
            mainlock.lock();
        }
    } catch (const std::exception& e) {
        TRACE(Trace::error, FuseFS::lockPath(path));
        return (-1 * EACCES);
    }

    if ((source = (fuse_bufvec*) malloc(sizeof(struct fuse_bufvec))) == NULL)
        return (-1 * errno);

    *source = FUSE_BUFVEC_INIT(size);
    source->buf[0].flags = (fuse_buf_flags) (FUSE_BUF_IS_FD | FUSE_BUF_FD_SEEK);
    source->buf[0].fd = linfo->fd;
    source->buf[0].pos = offset;
    *bufferp = source;

    return 0;
}

int FuseFS::ltfsdm_write_buf(const char *path, struct fuse_bufvec *buf,
        off_t offset, struct fuse_file_info *finfo)

{
    ssize_t wsize;
    FuseFS::mig_info migInfo;
    ssize_t attrsize;
    FuseFS::ltfsdm_file_info *linfo = (FuseFS::ltfsdm_file_info *) finfo->fh;

    struct fuse_bufvec dest = FUSE_BUFVEC_INIT(fuse_buf_size(buf));

    assert(path == NULL);

    if (linfo == NULL)
        return (-1 * EBADF);

    memset(&migInfo, 0, sizeof(FuseFS::mig_info));

    std::unique_lock<FuseLock> mainlock(*(linfo->main_lock));

    try {
        FuseLock trec_lock(FuseFS::lockPath(linfo->fusepath), FuseLock::fuse,
                FuseLock::lockexclusive);

        std::lock_guard<FuseLock> treclock(trec_lock);

        if ((attrsize = fgetxattr(linfo->fd,
                Const::OPEN_LTFS_EA_MIGINFO_INT.c_str(), (void *) &migInfo,
                sizeof(migInfo))) == -1) {
            if ( errno != ENODATA) {
                TRACE(Trace::error, errno);
                return (-1 * errno);
            }
        }

        if (migInfo.state == FuseFS::mig_info::state_num::MIGRATED
                || migInfo.state == FuseFS::mig_info::state_num::IN_RECALL) {
            TRACE(Trace::full, linfo->fd);
            mainlock.unlock();
            if (recall_file(linfo, true) == -1) {
                return (-1 * EIO);
            }
            mainlock.lock();
        } else if (migInfo.state == FuseFS::mig_info::state_num::PREMIGRATED) {
            if (fremovexattr(linfo->fd, Const::OPEN_LTFS_EA_MIGINFO_INT.c_str())
                    == -1) {
                TRACE(Trace::error, errno);
                return (-1 * EIO);
            }
            if (fremovexattr(linfo->fd, Const::OPEN_LTFS_EA_MIGINFO_EXT.c_str())
                    == -1) {
                TRACE(Trace::error, errno);
                return (-1 * EIO);
            }
        }
    } catch (const std::exception& e) {
        TRACE(Trace::error, FuseFS::lockPath(path));
        return (-1 * EACCES);
    }

    dest.buf[0].flags = (fuse_buf_flags) (FUSE_BUF_IS_FD | FUSE_BUF_FD_SEEK);
    dest.buf[0].fd = linfo->fd;
    dest.buf[0].pos = offset;

    if ((wsize = fuse_buf_copy(&dest, buf, FUSE_BUF_SPLICE_NONBLOCK)) == -1) {
        TRACE(Trace::error, errno);
        return (-1 * errno);
    } else {
        return wsize;
    }
}

int FuseFS::ltfsdm_statfs(const char *path, struct statvfs *stbuf)

{
    int fd;

    if ((fd = openat(getffs()->rootFd, FuseFS::relPath(path), O_RDONLY)) == -1)
        return (-1 * errno);

    if (fstatvfs(fd, stbuf) == -1) {
        close(fd);
        return (-1 * errno);
    } else {
        close(fd);
        return 0;
    }
}

int FuseFS::ltfsdm_release(const char *path, struct fuse_file_info *finfo)

{
    FuseFS::ltfsdm_file_info *linfo = (FuseFS::ltfsdm_file_info *) finfo->fh;
    bool failed = false;

    assert(path == NULL);

    if (linfo == NULL)
        return (-1 * EBADF);

    if (getffs()->rootFd == Const::UNSET
            && Const::OPEN_LTFS_IOCTL.compare(linfo->fusepath) == 0) {
        TRACE(Trace::always, linfo->fusepath);
        delete (linfo);
        return 0;
    }

    if (linfo->main_lock != nullptr)
        delete (linfo->main_lock);

    if (linfo->fd != Const::UNSET && close(linfo->fd) == -1)
        failed = true;

    if (linfo->lfd != Const::UNSET && close(linfo->lfd) == -1)
        failed = true;

    if (failed == true) {
        delete (linfo);
        return (-1 * errno);
    } else {
        delete (linfo);
        return 0;
    }
}

int FuseFS::ltfsdm_flush(const char *path, struct fuse_file_info *finfo)

{
    FuseFS::ltfsdm_file_info *linfo = (FuseFS::ltfsdm_file_info *) finfo->fh;

    assert(path == NULL);

    if (linfo == NULL)
        return (-1 * EBADF);

    if (close(dup(linfo->fd)) == -1)
        return (-1 * errno);
    else
        return 0;
}

int FuseFS::ltfsdm_fsync(const char *path, int isdatasync,
        struct fuse_file_info *finfo)

{
    FuseFS::ltfsdm_file_info *linfo = (FuseFS::ltfsdm_file_info *) finfo->fh;

    int rc;

    assert(path == NULL);

    if (linfo == NULL)
        return (-1 * EBADF);

    if (isdatasync)
        rc = fdatasync(linfo->fd);
    else
        rc = fsync(linfo->fd);

    if (rc == -1)
        return (-1 * errno);
    else
        return 0;
}

int FuseFS::ltfsdm_fallocate(const char *path, int mode, off_t offset,
        off_t length, struct fuse_file_info *finfo)

{
    FuseFS::ltfsdm_file_info *linfo = (FuseFS::ltfsdm_file_info *) finfo->fh;

    assert(path == NULL);

    if (linfo == NULL)
        return (-1 * EBADF);

    if (fallocate(linfo->fd, mode, offset, length) == -1)
        return (-1 * errno);
    else
        return 0;
}

int FuseFS::ltfsdm_setxattr(const char *path, const char *name,
        const char *value, size_t size, int flags)

{
    struct fuse_context *fc = fuse_get_context();
    pid_t pid = fc->pid;
    int fd;

    if (FuseFS::procIsOpenLTFS(pid) == false
            && std::string(name).find(Const::OPEN_LTFS_EA.c_str())
                    != std::string::npos)
        return (-1 * EPERM);

    if ((fd = openat(getffs()->rootFd, FuseFS::relPath(path), O_RDONLY)) == -1)
        return (-1 * errno);

    if (fsetxattr(fd, name, value, size, flags) == -1) {
        close(fd);
        return (-1 * errno);
    } else {
        close(fd);
        return 0;
    }
}

int FuseFS::ltfsdm_getxattr(const char *path, const char *name, char *value,
        size_t size)

{
    FuseFS::FuseHandle fh;
    ssize_t attrsize;
    int fd;

    if (Const::OPEN_LTFS_EA_FSINFO.compare(name) == 0) {
        assert(size == sizeof(fh));
        memset(&fh, 0, sizeof(fh));
        strncpy(fh.mountpoint, getffs()->mountpt.c_str(), PATH_MAX - 1);
        strncpy(fh.fusepath, relPath(path), PATH_MAX - 1);
        strncpy(fh.lockpath, FuseFS::lockPath(path).c_str(),
        PATH_MAX - 1);
        fh.fd = Const::UNSET;
        memcpy((void *) value, (void *) &fh, sizeof(fh));
        TRACE(Trace::always, fh.fusepath, sizeof(fh));
        return size;
    }

    if (Const::OPEN_LTFS_CACHE_DIR.compare(path) == 0)
        return (-1 * ENODATA);

    if ((fd = openat(getffs()->rootFd, FuseFS::relPath(path), O_RDONLY)) == -1)
        return (-1 * errno);

    if ((attrsize = fgetxattr(fd, name, value, size)) == -1) {
        close(fd);
        return (-1 * errno);
    }

    close(fd);

    TRACE(Trace::full, path, attrsize);
    return attrsize;
}

int FuseFS::ltfsdm_listxattr(const char *path, char *list, size_t size)

{
    ssize_t attrsize;
    int fd;

    if ((fd = openat(getffs()->rootFd, FuseFS::relPath(path), O_RDONLY)) == -1)
        return (-1 * errno);

    if ((attrsize = flistxattr(fd, list, size)) == -1) {
        close(fd);
        return (-1 * errno);
    } else {
        close(fd);
        return attrsize;
    }
}

int FuseFS::ltfsdm_removexattr(const char *path, const char *name)

{
    int fd;

    if (std::string(name).find(Const::OPEN_LTFS_EA.c_str())
            != std::string::npos)
        return (-1 * ENOTSUP);

    if ((fd = openat(getffs()->rootFd, FuseFS::relPath(path), O_RDONLY)) == -1)
        return (-1 * errno);

    if (fremovexattr(fd, name) == -1) {
        close(fd);
        return (-1 * errno);
    } else {
        close(fd);
        return 0;
    }
}

//static unsigned int igen;

int FuseFS::ltfsdm_ioctl(const char *path, int cmd, void *arg,
        struct fuse_file_info *finfo, unsigned int flags, void *data)

{
    FuseFS::FuseHandle fh;
    FuseFS::FuseHandle *fhp;
    FuseFS::ltfsdm_file_info *fi = (FuseFS::ltfsdm_file_info *) finfo->fh;
    unsigned int igen;

    TRACE(Trace::always, cmd);

    if (flags & FUSE_IOCTL_COMPAT)
        return -ENOSYS;

    switch (cmd) {
        case FuseFS::LTFSDM_FINFO:
            assert(_IOC_SIZE(cmd) == sizeof(fh));

            memset(&fh, 0, sizeof(fh));
            strncpy(fh.mountpoint, getffs()->mountpt.c_str(), PATH_MAX - 1);
            strncpy(fh.fusepath, relPath(fi->fusepath.c_str()), PATH_MAX - 1);
            strncpy(fh.lockpath, FuseFS::lockPath(fi->fusepath.c_str()).c_str(),
            PATH_MAX - 1);
            fh.fd = Const::UNSET;
            memcpy((void *) data, (void *) &fh, sizeof(fh));
            TRACE(Trace::always, fh.fusepath, sizeof(fh));
            return 0;
        case FuseFS::LTFSDM_PREMOUNT:
            return 0;
        case FuseFS::LTFSDM_POSTMOUNT:
            setRootFd(open(getffs()->srcdir.c_str(), O_RDONLY));
            TRACE(Trace::always, getffs()->rootFd, errno);
            if (getffs()->rootFd == -1)
                return (-1 * errno);
            return 0;
        case FuseFS::LTFSDM_STOP:
            if (getffs()->rootFd != Const::UNSET) {
                close(getffs()->rootFd);
                setRootFd(Const::UNSET);
            }
            return 0;
            // the lock ioctls currently not used
        case FuseFS::LTFSDM_LOCK:
            fhp = (FuseFS::FuseHandle *) data;
            fhp->lock = new FuseLock(lockPath(fhp->fusepath), FuseLock::main,
                    FuseLock::lockexclusive);
            fhp->lock->lock();
            return 0;
        case FuseFS::LTFSDM_TRYLOCK:
            fhp = (FuseFS::FuseHandle *) data;
            fhp->lock = new FuseLock(lockPath(fhp->fusepath), FuseLock::main,
                    FuseLock::lockexclusive);
            fhp->lock->try_lock();
            return 0;
        case FuseFS::LTFSDM_UNLOCK:
            fhp = (FuseFS::FuseHandle *) data;
            fhp->lock->unlock();
            delete (fhp->lock);
            return 0;
        case FS_IOC32_GETFLAGS:
            unsigned int iflags;
            if (ioctl(fi->fd, FS_IOC_GETFLAGS, &iflags) == -1) {
                *(unsigned int *) data = 0;
                return (-1 * errno);
            } else {
                *(unsigned int *) data = iflags;
                return 0;
            }
        case FS_IOC32_GETVERSION:
            if (ioctl(fi->fd, FS_IOC_GETVERSION, &igen) == -1) {
                *(unsigned int *) data = 0;
                return (-1 * errno);
            } else {
                *(unsigned int *) data = igen;
                return 0;
            }
        default:
            return (-1 * ENOTTY);
    }
}

void *FuseFS::ltfsdm_init(struct fuse_conn_info *conn)

{
    struct fuse_context *fc = fuse_get_context();

    return fc->private_data;
}

struct fuse_operations FuseFS::init_operations()

{
    struct fuse_operations ops;

    memset(&ops, 0, sizeof(struct fuse_operations));

    ops.init = FuseFS::ltfsdm_init;

    ops.getattr = FuseFS::ltfsdm_getattr;
    ops.access = FuseFS::ltfsdm_access;
    ops.readlink = FuseFS::ltfsdm_readlink;

    ops.opendir = FuseFS::ltfsdm_opendir;
    ops.readdir = FuseFS::ltfsdm_readdir;
    ops.releasedir = FuseFS::ltfsdm_releasedir;

    ops.mknod = FuseFS::ltfsdm_mknod;
    ops.mkdir = FuseFS::ltfsdm_mkdir;
    ops.symlink = FuseFS::ltfsdm_symlink;
    ops.unlink = FuseFS::ltfsdm_unlink;
    ops.rmdir = FuseFS::ltfsdm_rmdir;
    ops.rename = FuseFS::ltfsdm_rename;
    ops.link = FuseFS::ltfsdm_link;
    ops.chmod = FuseFS::ltfsdm_chmod;
    ops.chown = FuseFS::ltfsdm_chown;
    ops.truncate = FuseFS::ltfsdm_truncate;
    ops.utimens = FuseFS::ltfsdm_utimens;
    ops.open = FuseFS::ltfsdm_open;
    ops.ftruncate = FuseFS::ltfsdm_ftruncate;
    //ops.read			= FuseFS::ltfsdm_read;
    ops.read_buf = FuseFS::ltfsdm_read_buf;
    //ops.write			= FuseFS::ltfsdm_write;
    ops.write_buf = FuseFS::ltfsdm_write_buf;
    ops.statfs = FuseFS::ltfsdm_statfs;
    ops.release = FuseFS::ltfsdm_release;
    ops.flush = FuseFS::ltfsdm_flush;
    ops.fsync = FuseFS::ltfsdm_fsync;
    ops.fallocate = FuseFS::ltfsdm_fallocate;
    ops.setxattr = FuseFS::ltfsdm_setxattr;
    ops.getxattr = FuseFS::ltfsdm_getxattr;
    ops.listxattr = FuseFS::ltfsdm_listxattr;
    ops.removexattr = FuseFS::ltfsdm_removexattr;
    ops.ioctl = FuseFS::ltfsdm_ioctl;

    return ops;
}

void FuseFS::execute(std::string sourcedir, std::string mountpt,
        std::string command)

{
    int ret;

    pthread_setname_np(pthread_self(), "ltfsdmd.ofs");

    ret = system(command.c_str());

    if (!WIFEXITED(ret) || WEXITSTATUS(ret)) {
        TRACE(Trace::error, ret, WIFEXITED(ret), WEXITSTATUS(ret));
        MSG(LTFSDMF0023E, sourcedir, WEXITSTATUS(ret));
        umount2(mountpt.c_str(), MNT_FORCE | MNT_DETACH);
        kill(getpid(), SIGTERM);
    } else if (Connector::connectorTerminate == false) {
        MSG(LTFSDMF0030I, sourcedir);
        umount2(mountpt.c_str(), MNT_FORCE | MNT_DETACH);
        kill(getpid(), SIGTERM);
    }
}

void FuseFS::init(struct timespec starttime)

{
    std::stringstream stream;
    char exepath[PATH_MAX];
    char *tmpdnm;
    int fd = Const::UNSET;
    int count = 0;
    std::string devName = LTFSDM::getDev(mountpt);

    memset(exepath, 0, PATH_MAX);
    if (readlink("/proc/self/exe", exepath, PATH_MAX - 1) == -1) {
        MSG(LTFSDMC0021E);
        THROW(Error::LTFSDM_GENERAL_ERROR);
    }

    MSG(LTFSDMF0034I);
    memset(tmpdir, 0, sizeof(tmpdir));
    strncpy(tmpdir, Const::TMP_DIR_TEMPLATE.c_str(), PATH_MAX - 1);
    if ((tmpdnm = mkdtemp(tmpdir)) == NULL) {
        MSG(LTFSDMF0035E, errno);
        THROW(Error::LTFSDM_GENERAL_ERROR);
    }
    init_status.TEMP_CREATED = true;

    MSG(LTFSDMF0036I, mountpt.c_str(), tmpdnm);
    if (mount(mountpt.c_str(), tmpdnm, "", MS_BIND, "") == -1) {
        MSG(LTFSDMF0037E, tmpdnm, errno);
        THROW(Error::LTFSDM_GENERAL_ERROR);
    }
    init_status.TEMP_MOUNTED = true;

    MSG(LTFSDMF0038I, mountpt);
    if (umount(mountpt.c_str()) == -1) {
        MSG(LTFSDMF0039E, mountpt);
        THROW(Error::LTFSDM_GENERAL_ERROR);
    }

    stream << dirname(exepath) << "/" << Const::OVERLAY_FS_COMMAND << " -m "
            << mountpt << " -f " << devName << " -S " << starttime.tv_sec
            << " -N " << starttime.tv_nsec << " -l "
            << messageObject.getLogType() << " -t " << traceObject.getTrclevel()
            << " -p " << getpid() << " 2>&1";
    TRACE(Trace::always, stream.str());
    thrd = new std::thread(&FuseFS::execute,
            (mountpt + Const::OPEN_LTFS_CACHE_MP), mountpt, stream.str());

    while (true) {
        if ((fd = open((mountpt + Const::OPEN_LTFS_IOCTL).c_str(), O_RDONLY))
                == -1) {
            if (count < 20) {
                MSG(LTFSDMF0041I);
                sleep(1);
                count++;
                continue;
            }
            MSG(LTFSDMF0040E, errno);
            THROW(Error::LTFSDM_GENERAL_ERROR);
        } else if (ioctl(fd, FuseFS::LTFSDM_PREMOUNT) != -1) {
            break;
        } else if (count < 20) {
            MSG(LTFSDMF0041I);
            close(fd);
            sleep(1);
            count++;
        } else {
            close(fd);
            MSG(LTFSDMF0040E, errno);
            THROW(Error::LTFSDM_GENERAL_ERROR);
        }
    }

    init_status.FUSE_STARTED = true;
    FuseFS::ioctlFd = fd;

    MSG(LTFSDMF0036I, tmpdnm, mountpt + Const::OPEN_LTFS_CACHE_MP);
    if (mount(tmpdnm, (mountpt + Const::OPEN_LTFS_CACHE_MP).c_str(), "",
    MS_BIND, "") == -1) {
        MSG(LTFSDMF0037E, mountpt + Const::OPEN_LTFS_CACHE_MP, errno);
        THROW(Error::LTFSDM_GENERAL_ERROR);
    }
    init_status.CACHE_MOUNTED = true;

    MSG(LTFSDMF0042I, tmpdnm);
    if (umount(tmpdnm) == -1) {
        MSG(LTFSDMF0043E, errno);
        THROW(Error::LTFSDM_GENERAL_ERROR);
    }
    init_status.TEMP_MOUNTED = false;

    MSG(LTFSDMF0044I, tmpdnm);
    if (rmdir(tmpdnm) == -1) {
        MSG(LTFSDMF0045E, tmpdnm, errno);
        THROW(Error::LTFSDM_GENERAL_ERROR);
    }
    init_status.TEMP_CREATED = false;

    MSG(LTFSDMF0046I, mountpt + Const::OPEN_LTFS_CACHE_MP);
    if ((rootFd = open((mountpt + Const::OPEN_LTFS_CACHE_MP).c_str(),
    O_RDONLY)) == -1) {
        MSG(LTFSDMF0047E, errno);
        THROW(Error::LTFSDM_GENERAL_ERROR);
    }

    MSG(LTFSDMF0048I, mountpt);
    if (ioctl(fd, FuseFS::LTFSDM_POSTMOUNT) == -1) {
        MSG(LTFSDMF0049E, errno);
        THROW(Error::LTFSDM_GENERAL_ERROR);
    }
    init_status.ROOTFD_FUSE = true;

    MSG(LTFSDMF0050I, mountpt);
    if (umount2((mountpt + Const::OPEN_LTFS_CACHE_MP).c_str(), MNT_DETACH)
            == -1) {
        MSG(LTFSDMF0051E, mountpt, errno);
        THROW(Error::LTFSDM_GENERAL_ERROR);
    }
    init_status.CACHE_MOUNTED = false;
}

int main(int argc, char **argv)

{
    std::string mountpt("");
    std::string fsName("");
    struct timespec starttime = { 0, 0 };
    pid_t mainpid;
    Message::LogType logType;
    Trace::traceLevel tl;
    bool logTypeSet = false;
    bool traceLevelSet = false;
    int opt;
    opterr = 0;

    const struct fuse_operations ltfsdm_operations = FuseFS::init_operations();
    struct fuse_args fargs;
    std::stringstream options;

    while ((opt = getopt(argc, argv, "m:f:S:N:l:t:p:")) != -1) {
        switch (opt) {
            case 'm':
                if (mountpt.compare("") != 0)
                    return Error::LTFSDM_GENERAL_ERROR;
                mountpt = optarg;
                break;
            case 'f':
                if (fsName.compare("") != 0)
                    return Error::LTFSDM_GENERAL_ERROR;
                fsName = optarg;
                break;
            case 'S':
                if (starttime.tv_sec != 0)
                    return Error::LTFSDM_GENERAL_ERROR;
                starttime.tv_sec = std::stol(optarg, nullptr);
                break;
            case 'N':
                if (starttime.tv_nsec != 0)
                    return Error::LTFSDM_GENERAL_ERROR;
                starttime.tv_nsec = std::stol(optarg, nullptr);
                break;
            case 'l':
                if (logTypeSet)
                    return Error::LTFSDM_GENERAL_ERROR;
                logType = static_cast<Message::LogType>(std::stoi(optarg,
                        nullptr));
                logTypeSet = true;
                break;
            case 't':
                if (traceLevelSet)
                    return Error::LTFSDM_GENERAL_ERROR;
                tl = static_cast<Trace::traceLevel>(std::stoi(optarg, nullptr));
                traceLevelSet = true;
                break;
            case 'p':
                if (mainpid)
                    return Error::LTFSDM_GENERAL_ERROR;
                mainpid = static_cast<pid_t>(std::stoi(optarg, nullptr));
                break;
            default:
                return Error::LTFSDM_GENERAL_ERROR;
        }
    }

    if (optind != 15) {
        return Error::LTFSDM_GENERAL_ERROR;
    }

    std::string mp = mountpt;
    std::replace(mp.begin(), mp.end(), '/', '.');
    messageObject.init(mp);
    messageObject.setLogType(logType);

    MSG(LTFSDMF0031I);

    try {
        traceObject.init(mp);
        traceObject.setTrclevel(tl);
    } catch (const std::exception& e) {
        exit((int) Error::LTFSDM_GENERAL_ERROR);
    }

    FuseFS ffs(mountpt, mountpt + Const::OPEN_LTFS_CACHE_MP, starttime,
            LTFSDM::getkey(), mainpid);

    MSG(LTFSDMF0001I, mountpt + Const::OPEN_LTFS_CACHE_MP, mountpt);

    fargs = FUSE_ARGS_INIT(0, NULL);
    fuse_opt_add_arg(&fargs, argv[0]);
    fuse_opt_add_arg(&fargs, mountpt.c_str());
    options << "-ouse_ino,fsname=OpenLTFS:" << fsName
//            << ",nopath,default_permissions,allow_other,kernel_cache,hard_remove,max_background="
            << ",nopath,default_permissions,allow_other,kernel_cache,max_background="
            << Const::MAX_FUSE_BACKGROUND;
    fuse_opt_add_arg(&fargs, options.str().c_str());
    fuse_opt_add_arg(&fargs, "-f");
    if (getppid() != 1 && traceObject.getTrclevel() == Trace::full)
        fuse_opt_add_arg(&fargs, "-d");

    MSG(LTFSDMF0002I, mountpt.c_str());

    return fuse_main(fargs.argc, fargs.argv, &ltfsdm_operations, (void * ) &ffs);
}

FuseFS::~FuseFS()

{
    int rc = 0;

    try {
        if (init_status.ROOTFD_FUSE == true) {
            if (ioctl(FuseFS::ioctlFd, FuseFS::LTFSDM_STOP) == -1)
                MSG(LTFSDMF0052E, mountpt, errno);
        }

        if (rootFd != Const::UNSET)
            close(rootFd);

        if (FuseFS::ioctlFd != Const::UNSET)
            close(FuseFS::ioctlFd);

        if (init_status.CACHE_MOUNTED) {
            MSG(LTFSDMF0054I, Const::OPEN_LTFS_CACHE_MP);
            if (umount((mountpt + Const::OPEN_LTFS_CACHE_MP).c_str()) == -1) {
                MSG(LTFSDMF0053E, mountpt + Const::OPEN_LTFS_CACHE_MP, errno);
            }
        }

        if (init_status.TEMP_MOUNTED == true) {
            MSG(LTFSDMF0042I, tmpdir);
            if (umount(tmpdir) == -1) {
                MSG(LTFSDMF0043E, errno);
            }
        }

        if (init_status.TEMP_CREATED == true) {
            MSG(LTFSDMF0044I, tmpdir);
            if (rmdir(tmpdir) == -1) {
                MSG(LTFSDMF0045E, tmpdir, errno);
            }
        }

        if (init_status.FUSE_STARTED == false)
            return;

        MSG(LTFSDMF0028I, mountpt.c_str());
        MSG(LTFSDMF0007I);
        do {
            if (Connector::forcedTerminate)
                rc = umount2(mountpt.c_str(), MNT_FORCE | MNT_DETACH);
            else
                rc = umount(mountpt.c_str());
            if (rc == -1) {
                if ( errno == EBUSY) {
                    sleep(1);
                    continue;
                } else {
                    umount2(mountpt.c_str(), MNT_FORCE | MNT_DETACH);
                }
            }
            break;
        } while (true);
        MSG(LTFSDMF0029I, mountpt.c_str());
        thrd->join();
        delete (thrd);
    } catch (const std::exception& e) {
        TRACE(Trace::error, e.what());
        kill(getpid(), SIGTERM);
    }
}
