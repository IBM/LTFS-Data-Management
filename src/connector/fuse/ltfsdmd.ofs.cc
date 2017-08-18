#include <stdio.h> // for rename
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/mount.h>
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

struct openltfs_ctx
{
    char sourcedir[PATH_MAX];
    char mountpoint[PATH_MAX];
    struct timespec starttime;
};

Connector::rec_info_t FuseFS::recinfo_share = (Connector::rec_info_t ) { 0, 0,
                0, 0, 0, "" };
std::atomic<fuid_t> FuseFS::trecall_fuid((fuid_t ) { 0, 0, 0 });
std::atomic<bool> FuseFS::no_rec_event(false);
std::atomic<long> FuseFS::ltfsdmKey(Const::UNSET);

FuseFS::mig_info FuseFS::genMigInfo(const char *path,
        FuseFS::mig_info::state_num state)

{
    FuseFS::mig_info miginfo;

    memset(&miginfo, 0, sizeof(miginfo));

    if (stat(path, &miginfo.statinfo)) {
        TRACE(Trace::error, errno);
        THROW(errno, errno, path);
    }

    miginfo.state = state;

    clock_gettime(CLOCK_REALTIME, &miginfo.changed);

    return miginfo;
}

void FuseFS::setMigInfo(const char *path, FuseFS::mig_info::state_num state)

{
    ssize_t size;
    FuseFS::mig_info miginfo_new;
    FuseFS::mig_info miginfo;

    TRACE(Trace::full, path, state);

    miginfo_new = genMigInfo(path, state);

    memset(&miginfo, 0, sizeof(miginfo));

    if ((size = getxattr(path, Const::OPEN_LTFS_EA_MIGINFO_INT.c_str(),
            (void *) &miginfo, sizeof(miginfo))) == -1) {
        if ( errno != ENODATA) {
            THROW(errno, errno, path);
        }
    } else if (size != sizeof(miginfo)) {
        THROW(EIO, size, sizeof(miginfo), path);
    }

    if (miginfo.state != FuseFS::mig_info::state_num::NO_STATE) {
        miginfo_new.statinfo.st_size = miginfo.statinfo.st_size;
        miginfo_new.statinfo.st_atim = miginfo.statinfo.st_atim;
        miginfo_new.statinfo.st_mtim = miginfo.statinfo.st_mtim;
    }

    if (setxattr(path, Const::OPEN_LTFS_EA_MIGINFO_INT.c_str(),
            (void *) &miginfo_new, sizeof(miginfo_new), 0) == -1)
        THROW(errno, errno, path);
}

int FuseFS::remMigInfo(const char *path)

{
    TRACE(Trace::full, path);

    if (removexattr(path, Const::OPEN_LTFS_EA_MIGINFO_INT.c_str()) == -1)
        return errno;

    if (removexattr(path, Const::OPEN_LTFS_EA_MIGINFO_EXT.c_str()) == -1)
        return errno;

    return 0;
}

FuseFS::mig_info FuseFS::getMigInfo(const char *path)

{
    ssize_t size;
    FuseFS::mig_info miginfo;

    memset(&miginfo, 0, sizeof(miginfo));

    if ((size = getxattr(path, Const::OPEN_LTFS_EA_MIGINFO_INT.c_str(),
            (void *) &miginfo, sizeof(miginfo))) == -1) {
        // TODO
        /* check for errno */
        return miginfo;
    } else if (size != sizeof(miginfo)) {
        THROW(EIO, size, sizeof(miginfo), path);
    }

    return miginfo;
}

FuseFS::mig_info FuseFS::getMigInfoAt(int dirfd, const char *path)

{
    ssize_t size;
    FuseFS::mig_info miginfo;
    int fd;

    memset(&miginfo, 0, sizeof(miginfo));

    if ((fd = openat(dirfd, path, O_RDONLY)) == -1)
        THROW(errno, errno, dirfd);

    if ((size = fgetxattr(fd, Const::OPEN_LTFS_EA_MIGINFO_INT.c_str(),
            (void *) &miginfo, sizeof(miginfo))) == -1) {
        // TODO
        /* check for errno */
        close(fd);
        return miginfo;
    }

    close(fd);

    if (size != sizeof(miginfo))
        THROW(EIO, size, sizeof(miginfo), fd);

    return miginfo;
}

bool FuseFS::needsRecovery(FuseFS::mig_info miginfo)

{
    struct fuse_context *fc = fuse_get_context();

    if ((miginfo.state == FuseFS::mig_info::state_num::IN_MIGRATION)
            | (miginfo.state == FuseFS::mig_info::state_num::STUBBING)
            | (miginfo.state == FuseFS::mig_info::state_num::IN_RECALL)) {

        if (((openltfs_ctx *) fc->private_data)->starttime.tv_sec
                < miginfo.changed.tv_sec)
            return false;
        else if ((((openltfs_ctx *) fc->private_data)->starttime.tv_sec
                == miginfo.changed.tv_sec)
                && (((openltfs_ctx *) fc->private_data)->starttime.tv_nsec
                        < miginfo.changed.tv_nsec))
            return false;
        else
            return true;
    }

    return false;
}

void FuseFS::recoverState(const char *path, FuseFS::mig_info::state_num state)

{
    struct fuse_context *fc = fuse_get_context();

    std::string fusepath = ((openltfs_ctx *) fc->private_data)->mountpoint
            + std::string(path);
    std::string sourcepath = ((openltfs_ctx *) fc->private_data)->sourcedir
            + std::string(path);

    TRACE(Trace::error, fusepath, state);

    FsObj file(sourcepath);

    switch (state) {
        case FuseFS::mig_info::state_num::IN_MIGRATION:
            MSG(LTFSDMF0013W, fusepath);
            if (removexattr(sourcepath.c_str(),
                    Const::OPEN_LTFS_EA_MIGINFO_EXT.c_str()) == -1) {
                TRACE(Trace::error, errno);
                MSG(LTFSDMF0018W, Const::OPEN_LTFS_EA_MIGINFO_EXT);
            }
            if (removexattr(sourcepath.c_str(),
                    Const::OPEN_LTFS_EA_MIGINFO_INT.c_str()) == -1) {
                TRACE(Trace::error, errno);
                MSG(LTFSDMF0018W, Const::OPEN_LTFS_EA_MIGINFO_INT);
            }
            break;
        case FuseFS::mig_info::state_num::STUBBING:
            MSG(LTFSDMF0014W, fusepath);
            FuseFS::setMigInfo(sourcepath.c_str(),
                    FuseFS::mig_info::state_num::MIGRATED);
            if (truncate(sourcepath.c_str(), 0) == -1) {
                FuseFS::setMigInfo(sourcepath.c_str(),
                        FuseFS::mig_info::state_num::PREMIGRATED);
                MSG(LTFSDMF0016E, fusepath);
            }
            break;
        case FuseFS::mig_info::state_num::IN_RECALL:
            MSG(LTFSDMF0015W, fusepath);
            FuseFS::setMigInfo(sourcepath.c_str(),
                    FuseFS::mig_info::state_num::MIGRATED);
            if (truncate(sourcepath.c_str(), 0) == -1) {
                FuseFS::setMigInfo(sourcepath.c_str(),
                        FuseFS::mig_info::state_num::PREMIGRATED);
                MSG(LTFSDMF0016E, fusepath);
            }
            break;
        default:
            assert(0);
    }
}

std::string FuseFS::source_path(const char *path)

{
    std::string fullpath;
    FuseFS::mig_info miginfo;

    struct fuse_context *fc = fuse_get_context();
    fullpath = ((openltfs_ctx *) fc->private_data)->sourcedir
            + std::string(path);

    try {
        miginfo = getMigInfo(fullpath.c_str());
    } catch (const OpenLTFSException& e) {
        TRACE(Trace::error, e.getError());
        MSG(LTFSDMF0011E, fullpath);
        return std::string("");
    } catch (const std::exception& e) {
        MSG(LTFSDMF0011E, fullpath);
        return std::string("");
    }
    if (FuseFS::needsRecovery(miginfo) == true)
        FuseFS::recoverState(path, miginfo.state);

    return fullpath;
}

int FuseFS::recall_file(FuseFS::ltfsdm_file_info *linfo, bool toresident)

{
    struct stat statbuf;
    unsigned int igen;
    bool success;

    if (fstat(linfo->fd, &statbuf) == -1) {
        TRACE(Trace::error, fuse_get_context()->pid, errno);
        return (-1 * errno);
    }

    if (ioctl(linfo->fd, FS_IOC_GETVERSION, &igen)) {
        TRACE(Trace::error, fuse_get_context()->pid, errno);
        return (-1 * errno);
    }

    TRACE(Trace::always, linfo->sourcepath, statbuf.st_ino, toresident);

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

    recrequest->set_key(FuseFS::ltfsdmKey);
    recrequest->set_toresident(toresident);
    recrequest->set_fsid(statbuf.st_dev);
    recrequest->set_igen(igen);
    recrequest->set_ino(statbuf.st_ino);
    recrequest->set_filename(linfo->sourcepath);

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

    TRACE(Trace::always, linfo->sourcepath, statbuf.st_ino, success);

    if (success == false)
        return -1;
    else
        return 0;
}

int FuseFS::ltfsdm_getattr(const char *path, struct stat *statbuf)

{
    FuseFS::mig_info miginfo;

    memset(statbuf, 0, sizeof(struct stat));

    if (lstat(FuseFS::source_path(path).c_str(), statbuf) == -1) {
        return (-1 * errno);
    } else {
        miginfo = getMigInfo(FuseFS::source_path(path).c_str());
        if (miginfo.state != FuseFS::mig_info::state_num::NO_STATE) {
            statbuf->st_size = miginfo.statinfo.st_size;
            statbuf->st_atim = miginfo.statinfo.st_atim;
            statbuf->st_mtim = miginfo.statinfo.st_mtim;
        }
        return 0;
    }
}

int FuseFS::ltfsdm_access(const char *path, int mask)

{
    if (access(FuseFS::source_path(path).c_str(), mask) == -1)
        return (-1 * errno);
    else
        return 0;
}

int FuseFS::ltfsdm_readlink(const char *path, char *buffer, size_t size)

{
    memset(buffer, 0, size);

    if (readlink(FuseFS::source_path(path).c_str(), buffer, size - 1) == -1)
        return (-1 * errno);
    else
        return 0;
}

int FuseFS::ltfsdm_opendir(const char *path, struct fuse_file_info *finfo)

{
    FuseFS::ltfsdm_dir_info *dirinfo = NULL;
    DIR *dir = NULL;

    if ((dir = opendir(FuseFS::source_path(path).c_str())) == 0) {
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
            miginfo = getMigInfoAt(dirfd(dirinfo->dir),
                    dirinfo->dentry->d_name);
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
    struct fuse_context *fc;

    if (mknod(FuseFS::source_path(path).c_str(), mode, rdev) == -1) {
        return (-1 * errno);
    } else {
        fc = fuse_get_context();
        if (chown(FuseFS::source_path(path).c_str(), fc->uid, fc->gid) == -1)
            return (-1 * errno);
    }
    return 0;
}

int FuseFS::ltfsdm_mkdir(const char *path, mode_t mode)

{
    struct fuse_context *fc;

    if (mkdir(FuseFS::source_path(path).c_str(), mode) == -1) {
        return (-1 * errno);
    } else {
        fc = fuse_get_context();
        if (chown(FuseFS::source_path(path).c_str(), fc->uid, fc->gid) == -1)
            return (-1 * errno);
    }
    return 0;
}

int FuseFS::ltfsdm_unlink(const char *path)

{
    if (unlink(FuseFS::source_path(path).c_str()) == -1)
        return (-1 * errno);
    else
        return 0;
}

int FuseFS::ltfsdm_rmdir(const char *path)

{
    if (rmdir(FuseFS::source_path(path).c_str()) == -1)
        return (-1 * errno);
    else
        return 0;
}

int FuseFS::ltfsdm_symlink(const char *target, const char *linkpath)

{
    if (symlink(target, FuseFS::source_path(linkpath).c_str()) == -1)
        return (-1 * errno);
    else
        return 0;
}

int FuseFS::ltfsdm_rename(const char *oldpath, const char *newpath)

{
    if (rename(FuseFS::source_path(oldpath).c_str(),
            FuseFS::source_path(newpath).c_str()) == -1)
        return (-1 * errno);
    else
        return 0;
}

int FuseFS::ltfsdm_link(const char *oldpath, const char *newpath)

{
    if (link(FuseFS::source_path(oldpath).c_str(),
            FuseFS::source_path(newpath).c_str()) == -1)
        return (-1 * errno);
    else
        return 0;
}

int FuseFS::ltfsdm_chmod(const char *path, mode_t mode)

{
    if (chmod(FuseFS::source_path(path).c_str(), mode) == -1)
        return (-1 * errno);
    else
        return 0;
}

int FuseFS::ltfsdm_chown(const char *path, uid_t uid, gid_t gid)

{
    if (lchown(FuseFS::source_path(path).c_str(), uid, gid) == -1)
        return (-1 * errno);
    else
        return 0;
}

int FuseFS::ltfsdm_truncate(const char *path, off_t size)

{
    FuseFS::mig_info migInfo;
    ssize_t attrsize;
    FuseFS::ltfsdm_file_info linfo = (FuseFS::ltfsdm_file_info ) { 0, "", "" };

    linfo.fusepath = path;
    linfo.sourcepath = FuseFS::source_path(path);

    if ((linfo.fd = open(linfo.sourcepath.c_str(), O_WRONLY)) == -1) {
        TRACE(Trace::error, errno);
        return (-1 * errno);
    }

    memset(&migInfo, 0, sizeof(FuseFS::mig_info));

    if ((attrsize = fgetxattr(linfo.fd, Const::OPEN_LTFS_EA_MIGINFO_INT.c_str(),
            (void *) &migInfo, sizeof(migInfo))) == -1) {
        if ( errno != ENODATA) {
            close(linfo.fd);
            TRACE(Trace::error, fuse_get_context()->pid);
            return (-1 * errno);
        }
    }

    if ((size > 0)
            && ((migInfo.state == FuseFS::mig_info::state_num::MIGRATED)
                    || (migInfo.state == FuseFS::mig_info::state_num::IN_RECALL))) {
        TRACE(Trace::full, path);
        if (recall_file(&linfo, true) == -1) {
            close(linfo.fd);
            return (-1 * EIO);
        }
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

    if (ftruncate(linfo.fd, size) == -1) {
        close(linfo.fd);
        return (-1 * errno);
    }

    close(linfo.fd);

    return 0;
}

int FuseFS::ltfsdm_utimens(const char *path, const struct timespec times[2])

{
    if (utimensat(0, FuseFS::source_path(path).c_str(), times,
    AT_SYMLINK_NOFOLLOW) == -1)
        return (-1 * errno);
    else
        return 0;
}

int FuseFS::ltfsdm_open(const char *path, struct fuse_file_info *finfo)

{
    int fd = -1;
    FuseFS::ltfsdm_file_info *linfo = NULL;

    if ((fd = open(FuseFS::source_path(path).c_str(), finfo->flags)) == -1) {
        TRACE(Trace::error, fuse_get_context()->pid);
        return (-1 * errno);
    }

    linfo = new (FuseFS::ltfsdm_file_info);
    linfo->fd = fd;
    linfo->fusepath = path;
    linfo->sourcepath = FuseFS::source_path(path);

    finfo->fh = (unsigned long) linfo;

    return 0;
}

int FuseFS::ltfsdm_ftruncate(const char *path, off_t size,
        struct fuse_file_info *finfo)

{
    FuseFS::mig_info migInfo;
    ssize_t attrsize;
    FuseFS::ltfsdm_file_info *linfo = (FuseFS::ltfsdm_file_info *) finfo->fh;

    // ftruncate provides a path name
    // assert(path == NULL);

    if (linfo == NULL) {
        TRACE(Trace::error, fuse_get_context()->pid);
        return (-1 * EBADF);
    }

    memset(&migInfo, 0, sizeof(FuseFS::mig_info));

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
                    || (migInfo.state == FuseFS::mig_info::state_num::IN_RECALL))) {
        TRACE(Trace::full, linfo->fd);
        if (recall_file(linfo, true) == -1) {
            return (-1 * EIO);
        }
    } else if (attrsize != -1) {
        if (fremovexattr(linfo->fd, Const::OPEN_LTFS_EA_MIGINFO_INT.c_str())
                == -1)
            return (-1 * EIO);
        if (fremovexattr(linfo->fd, Const::OPEN_LTFS_EA_MIGINFO_EXT.c_str())
                == -1)
            return (-1 * EIO);
    }

    if (ftruncate(linfo->fd, size) == -1) {
        return (-1 * errno);
    }

    return 0;
}

int FuseFS::ltfsdm_read(const char *path, char *buffer, size_t size,
        off_t offset, struct fuse_file_info *finfo)

{
    ssize_t rsize = -1;
    FuseFS::mig_info migInfo;
    ssize_t attrsize;
    FuseFS::ltfsdm_file_info *linfo = (FuseFS::ltfsdm_file_info *) finfo->fh;

    assert(path == NULL);

    if (linfo == NULL) {
        TRACE(Trace::error, fuse_get_context()->pid);
        return (-1 * EBADF);
    }

    memset(&migInfo, 0, sizeof(FuseFS::mig_info));

    if ((attrsize = fgetxattr(linfo->fd,
            Const::OPEN_LTFS_EA_MIGINFO_INT.c_str(), (void *) &migInfo,
            sizeof(migInfo))) == -1) {
        if ( errno != ENODATA) {
            TRACE(Trace::error, fuse_get_context()->pid);
            return (-1 * errno);
        }
    }

    if (migInfo.state == FuseFS::mig_info::state_num::MIGRATED
            || migInfo.state == FuseFS::mig_info::state_num::IN_RECALL) {
        TRACE(Trace::full, linfo->fd);
        if (recall_file(linfo, false) == -1)
            return (-1 * EIO);
    }

    if ((rsize = pread(linfo->fd, buffer, size, offset)) == -1) {
        TRACE(Trace::error, fuse_get_context()->pid);
        return (-1 * errno);
    } else {
        TRACE(Trace::normal, fuse_get_context()->pid);
        return rsize;
    }
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
        if (recall_file(linfo, false) == -1) {
            *bufferp = NULL;
            return (-1 * EIO);
        }
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

int FuseFS::ltfsdm_write(const char *path, const char *buf, size_t size,
        off_t offset, struct fuse_file_info *finfo)

{
    ssize_t wsize;
    FuseFS::mig_info migInfo;
    ssize_t attrsize;
    FuseFS::ltfsdm_file_info *linfo = (FuseFS::ltfsdm_file_info *) finfo->fh;

    assert(path == NULL);

    if (linfo == NULL)
        return (-1 * EBADF);

    memset(&migInfo, 0, sizeof(FuseFS::mig_info));

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
        if (recall_file(linfo, true) == -1) {
            return (-1 * EIO);
        }
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

    if ((wsize = pwrite(linfo->fd, buf, size, offset)) == -1) {
        TRACE(Trace::error, errno);
        return (-1 * errno);
    } else {
        return wsize;
    }
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
        if (recall_file(linfo, true) == -1) {
            return (-1 * EIO);
        }
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
    if (statvfs(FuseFS::source_path(path).c_str(), stbuf) == -1)
        return (-1 * errno);
    else
        return 0;
}

int FuseFS::ltfsdm_release(const char *path, struct fuse_file_info *finfo)

{
    FuseFS::ltfsdm_file_info *linfo = (FuseFS::ltfsdm_file_info *) finfo->fh;

    assert(path == NULL);

    if (linfo == NULL)
        return (-1 * EBADF);

    if (close(linfo->fd) == -1) {
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
    if (std::string(name).find(Const::OPEN_LTFS_EA.c_str())
            != std::string::npos)
        return (-1 * ENOTSUP);

    if (lsetxattr(FuseFS::source_path(path).c_str(), name, value, size, flags)
            == -1)
        return (-1 * errno);
    else
        return 0;
}

int FuseFS::ltfsdm_getxattr(const char *path, const char *name, char *value,
        size_t size)

{
    ssize_t attrsize;
    //struct fuse_context *fc = fuse_get_context();

    // additionally to compare fc->pid with getpid() does not work
    // since fc->pid returns a thread id

    if (Const::OPEN_LTFS_EA_FSINFO.compare(name) == 0) {
        strncpy(value, FuseFS::source_path(path).c_str(), PATH_MAX - 1);
        size = strlen(value) + 1;
        TRACE(Trace::full, path, size);
        return size;
    } else if ((attrsize = lgetxattr(FuseFS::source_path(path).c_str(), name,
            value, size)) == -1) {
        return (-1 * errno);
    } else if (std::string(name).find(Const::OPEN_LTFS_EA.c_str())
            != std::string::npos) {
        return (-1 * ENOTSUP);
    } else {
        TRACE(Trace::full, path, attrsize);
        return attrsize;
    }
}

int FuseFS::ltfsdm_listxattr(const char *path, char *list, size_t size)

{
    ssize_t attrsize;

    if ((attrsize = llistxattr(FuseFS::source_path(path).c_str(), list, size))
            == -1)
        return (-1 * errno);
    else
        return attrsize;
}

int FuseFS::ltfsdm_removexattr(const char *path, const char *name)

{
    if (std::string(name).find(Const::OPEN_LTFS_EA.c_str())
            != std::string::npos)
        return (-1 * ENOTSUP);

    if (lremovexattr(FuseFS::source_path(path).c_str(), name) == -1)
        return (-1 * errno);
    else
        return 0;
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

    return ops;
}

void FuseFS::execute(std::string sourcedir, std::string command)

{
    int ret;

    pthread_setname_np(pthread_self(), "ltfsdmd.ofs");

    ret = system(command.c_str());

    if (!WIFEXITED(ret) || WEXITSTATUS(ret)) {
        TRACE(Trace::error, ret, WIFEXITED(ret), WEXITSTATUS(ret));
        MSG(LTFSDMF0023E, sourcedir, WEXITSTATUS(ret));
        kill(getpid(), SIGTERM);
    } else if (Connector::connectorTerminate == false) {
        MSG(LTFSDMF0030I, sourcedir);
        kill(getpid(), SIGTERM);
    }
}

FuseFS::FuseFS(std::string sourcedir, std::string mountpt, std::string fsName,
        struct timespec starttime) :
        mountpt(mountpt)

{
    std::stringstream stream;
    char exepath[PATH_MAX];

    memset(exepath, 0, PATH_MAX);
    if (readlink("/proc/self/exe", exepath, PATH_MAX - 1) == -1) {
        MSG(LTFSDMC0021E);
        THROW(Error::LTFSDM_GENERAL_ERROR);
    }

    stream << dirname(exepath) << "/" << Const::OVERLAY_FS_COMMAND << " -s "
            << sourcedir << " -m " << mountpt << " -f " << fsName << " -S "
            << starttime.tv_sec << " -N " << starttime.tv_nsec << " -l "
            << messageObject.getLogType() << " -t " << traceObject.getTrclevel()
            << " 2>&1";

    TRACE(Trace::always, stream.str());
    thrd = new std::thread(&FuseFS::execute, sourcedir, stream.str());
}

int main(int argc, char **argv)

{
    std::string sourcedir("");
    std::string mountpt("");
    std::string fsName("");
    struct timespec starttime = { 0, 0 };
    Message::LogType logType;
    Trace::traceLevel tl;
    bool logTypeSet = false;
    bool traceLevelSet = false;
    int opt;
    opterr = 0;

    const struct fuse_operations ltfsdm_operations = FuseFS::init_operations();
    struct openltfs_ctx *ctx;
    struct fuse_args fargs;
    std::stringstream options;

    while ((opt = getopt(argc, argv, "s:m:f:S:N:l:t:")) != -1) {
        switch (opt) {
            case 's':
                if (sourcedir.compare("") != 0)
                    return Error::LTFSDM_GENERAL_ERROR;
                sourcedir = optarg;
                break;
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

    traceObject.init(mp);
    traceObject.setTrclevel(tl);

    FuseFS::ltfsdmKey = LTFSDM::getkey();

    ctx = (openltfs_ctx *) malloc(sizeof(struct openltfs_ctx));
    memset(ctx, 0, sizeof(struct openltfs_ctx));
    strncpy(ctx->sourcedir, sourcedir.c_str(), PATH_MAX - 1);
    strncpy(ctx->mountpoint, mountpt.c_str(), PATH_MAX - 1);
    ctx->starttime = starttime;

    MSG(LTFSDMF0001I, ctx->sourcedir, mountpt);

    fargs = FUSE_ARGS_INIT(0, NULL);
    fuse_opt_add_arg(&fargs, argv[0]);
    fuse_opt_add_arg(&fargs, mountpt.c_str());
    options << "-ouse_ino,fsname=OpenLTFS:" << fsName
            << ",nopath,default_permissions,allow_other,max_background="
            << Const::MAX_FUSE_BACKGROUND;
    fuse_opt_add_arg(&fargs, options.str().c_str());
    fuse_opt_add_arg(&fargs, "-f");
    if (getppid() != 1 && traceObject.getTrclevel() == Trace::full)
        fuse_opt_add_arg(&fargs, "-d");

    MSG(LTFSDMF0002I, mountpt.c_str());

    return fuse_main(fargs.argc, fargs.argv, &ltfsdm_operations, (void * ) ctx);
}

FuseFS::~FuseFS()

{
    int rc = 0;

    try {
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
    } catch (...) {
        kill(getpid(), SIGTERM);
    }
}
