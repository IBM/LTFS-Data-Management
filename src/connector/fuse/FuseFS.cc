/*******************************************************************************
 * Copyright 2018 IBM Corp. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *  https://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 *******************************************************************************/
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
#include <libmount/libmount.h>
#include <blkid/blkid.h>
#include <errno.h>

#include <string>
#include <sstream>
#include <set>
#include <map>
#include <vector>
#include <atomic>
#include <condition_variable>
#include <thread>
#include <mutex>
#include <exception>

#include "src/common/errors.h"
#include "src/common/LTFSDMException.h"
#include "src/common/util.h"
#include "src/common/FileSystems.h"
#include "src/common/Message.h"
#include "src/common/Trace.h"
#include "src/common/Const.h"
#include "src/common/Configuration.h"

#include "src/communication/ltfsdm.pb.h"
#include "src/communication/LTFSDmComm.h"

#include "src/connector/Connector.h"
#include "src/connector/fuse/FuseLock.h"
#include "src/connector/fuse/FuseFS.h"

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

    if (fstatat(getshrd()->rootFd, FuseFS::relPath(path.c_str()), &statbuf,
    AT_SYMLINK_NOFOLLOW) == -1) {
        TRACE(Trace::error, path, errno);
        return "";
    }

    lpath << getshrd()->mountpt << Const::LTFSDM_LOCK_DIR << "/"
            << statbuf.st_ino;

    TRACE(Trace::always, lpath.str());

    return lpath.str();
}

FuseFS::mig_state_attr_t FuseFS::genMigInfoAt(int fd, FuseFS::mig_state_attr_t::state_num state)

{
    FuseFS::mig_state_attr_t miginfo;
    struct stat statbuf;

    memset(&miginfo, 0, sizeof(miginfo));

    if (fstat(fd, &statbuf)) {
        TRACE(Trace::error, errno);
        THROW(Error::GENERAL_ERROR, errno, fd);
    }

    miginfo.typeId = typeid(FuseFS::mig_state_attr_t).hash_code();
    miginfo.size = statbuf.st_size;
    miginfo.atime = statbuf.st_atim;
    miginfo.mtime = statbuf.st_mtim;
    miginfo.state = state;

    clock_gettime(CLOCK_REALTIME, &miginfo.changed);

    return miginfo;
}

void FuseFS::setMigInfoAt(int fd, FuseFS::mig_state_attr_t::state_num state)

{
    ssize_t size;
    FuseFS::mig_state_attr_t miginfo_new;
    FuseFS::mig_state_attr_t miginfo;

    TRACE(Trace::full, fd, state);

    memset(&miginfo, 0, sizeof(miginfo));

    miginfo_new = genMigInfoAt(fd, state);

    if ((size = fgetxattr(fd, Const::LTFSDM_EA_MIGSTATE.c_str(),
            (void *) &miginfo, sizeof(miginfo))) == -1) {
        if ( errno != ENODATA) {
            THROW(Error::GENERAL_ERROR, errno, fd);
        }
    } else if (size != sizeof(miginfo)
            || miginfo.typeId != typeid(FuseFS::mig_state_attr_t).hash_code()) {
        errno = EIO;
        THROW(Error::GENERAL_ERROR, size, sizeof(miginfo), miginfo.typeId,
                typeid(FuseFS::mig_state_attr_t).hash_code(), fd);
    }

    if (miginfo.state != FuseFS::mig_state_attr_t::state_num::RESIDENT) {
        // keep the previous settings
        miginfo_new.size = miginfo.size;
        miginfo_new.atime = miginfo.atime;
        miginfo_new.mtime = miginfo.mtime;
    }

    if (fsetxattr(fd, Const::LTFSDM_EA_MIGSTATE.c_str(), (void *) &miginfo_new,
            sizeof(miginfo_new), 0) == -1) {
        THROW(Error::GENERAL_ERROR, errno, fd);
    }
}

int FuseFS::remMigInfoAt(int fd)

{
    TRACE(Trace::full, fd);

    if (fremovexattr(fd, Const::LTFSDM_EA_MIGSTATE.c_str()) == -1) {
        TRACE(Trace::error, errno);
        return errno;
    }

    if (fremovexattr(fd, Const::LTFSDM_EA_MIGINFO.c_str()) == -1) {
        TRACE(Trace::error, errno);
        return errno;
    }

    return 0;
}

FuseFS::mig_state_attr_t FuseFS::getMigInfoAt(int fd)

{
    ssize_t size;
    FuseFS::mig_state_attr_t miginfo;

    memset(&miginfo, 0, sizeof(miginfo));

    if ((size = fgetxattr(fd, Const::LTFSDM_EA_MIGSTATE.c_str(),
            (void *) &miginfo, sizeof(miginfo))) == -1) {
        // TODO
        /* check for errno */
        return miginfo;
    }

    if (size != sizeof(miginfo)
            || miginfo.typeId != typeid(FuseFS::mig_state_attr_t).hash_code()) {
        errno = EIO;
        THROW(Error::ATTR_FORMAT, size, sizeof(miginfo), miginfo.typeId,
                typeid(FuseFS::mig_state_attr_t).hash_code(), fd);
    }

    return miginfo;
}

bool FuseFS::needsRecovery(FuseFS::mig_state_attr_t miginfo)

{
    if ((miginfo.state == FuseFS::mig_state_attr_t::state_num::IN_MIGRATION)
            | (miginfo.state == FuseFS::mig_state_attr_t::state_num::STUBBING)
            | (miginfo.state == FuseFS::mig_state_attr_t::state_num::IN_RECALL)) {

        if (getshrd()->starttime.tv_sec < miginfo.changed.tv_sec)
            return false;
        else if ((getshrd()->starttime.tv_sec == miginfo.changed.tv_sec)
                && (getshrd()->starttime.tv_nsec < miginfo.changed.tv_nsec))
            return false;
        else
            return true;
    }

    return false;
}

void FuseFS::recoverState(const char *path, FuseFS::mig_state_attr_t::state_num state)

{
    int fd;

    if ((fd = openat(getshrd()->rootFd, FuseFS::relPath(path), O_RDWR)) == -1) {
        TRACE(Trace::error, errno);
        MSG(LTFSDMF0010E, path);
        return;
    }

    std::string fusepath = getshrd()->mountpt + std::string(path);

    TRACE(Trace::error, fusepath, state);

    switch (state) {
        case FuseFS::mig_state_attr_t::state_num::IN_MIGRATION:
        case FuseFS::mig_state_attr_t::state_num::RESIDENT:
            MSG(LTFSDMF0013W, fusepath);
            if (fremovexattr(fd, Const::LTFSDM_EA_MIGINFO.c_str()) == -1) {
                TRACE(Trace::error, errno);
                if ( errno != ENODATA)
                    MSG(LTFSDMF0018W, Const::LTFSDM_EA_MIGINFO);
            }
            if (fremovexattr(fd, Const::LTFSDM_EA_MIGSTATE.c_str()) == -1) {
                TRACE(Trace::error, errno);
                if ( errno != ENODATA)
                    MSG(LTFSDMF0018W, Const::LTFSDM_EA_MIGSTATE);
            }
            break;
        case FuseFS::mig_state_attr_t::state_num::STUBBING:
            MSG(LTFSDMF0014W, fusepath);
            try {
                FuseFS::setMigInfoAt(fd, FuseFS::mig_state_attr_t::state_num::MIGRATED);
            } catch (const std::exception& e) {
                MSG(LTFSDMF0056E, fusepath);
            }
            if (ftruncate(fd, 0) == -1) {
                TRACE(Trace::error, errno);
                MSG(LTFSDMF0016E, fusepath);
                try {
                    FuseFS::setMigInfoAt(fd,
                            FuseFS::mig_state_attr_t::state_num::PREMIGRATED);
                } catch (const std::exception& e) {
                    MSG(LTFSDMF0056E, fusepath);
                }
            }
            break;
        case FuseFS::mig_state_attr_t::state_num::IN_RECALL:
            MSG(LTFSDMF0015W, fusepath);
            try {
                FuseFS::setMigInfoAt(fd, FuseFS::mig_state_attr_t::state_num::MIGRATED);
            } catch (const std::exception& e) {
                MSG(LTFSDMF0056E, fusepath);
            }
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

    path = getshrd()->mountpt;
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

    recrequest->set_key(getshrd()->ltfsdmKey);
    recrequest->set_toresident(toresident);
    recrequest->set_fsidh(getshrd()->fsid_h);
    recrequest->set_fsidl(getshrd()->fsid_l);
    recrequest->set_igen(igen);
    recrequest->set_inum(statbuf.st_ino);
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

bool FuseFS::procIsLTFSDM(pid_t tid)
{
    struct stat statbuf;

    pid_t pids[] = { getshrd()->mainpid, getpid() };

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
    FuseFS::mig_state_attr_t miginfo;
    struct fuse_context *fc = fuse_get_context();
    pid_t pid = fc->pid;
    int fd;

    memset(statbuf, 0, sizeof(struct stat));

    TRACE(Trace::always, pid, FuseFS::procIsLTFSDM(pid), path);

    while (getshrd()->rootFd == Const::UNSET
            && FuseFS::procIsLTFSDM(pid) == false)
        sleep(1);

    if (getshrd()->rootFd == Const::UNSET && FuseFS::procIsLTFSDM(pid) == true) {
        if (Const::LTFSDM_IOCTL.compare(path) == 0) {
            statbuf->st_mode = S_IFREG | S_IRWXU;
            goto end;
        }

        if (std::string("/").compare(path) == 0
                || Const::LTFSDM_CACHE_DIR.compare(path) == 0
                || Const::LTFSDM_CACHE_MP.compare(path) == 0) {
            statbuf->st_mode = S_IFDIR | S_IRWXU;
            goto end;
        }
    }

    if (std::string("/").compare(path) == 0) {
        if (fstatat(getshrd()->rootFd, ".", statbuf, AT_SYMLINK_NOFOLLOW)
                == -1) {
            statbuf->st_mode = S_IFDIR | S_IRWXU;
            goto end;
        }
        goto end;
    }

    if (FuseFS::procIsLTFSDM(pid) == true
            && std::string(Const::LTFSDM_CACHE_DIR).compare(path) == 0) {
        statbuf->st_mode = S_IFDIR | S_IRWXU;
        goto end;
    }

    if (std::string(path).compare(0, Const::LTFSDM_LOCK_DIR.size(),
            Const::LTFSDM_LOCK_DIR) == 0) {
        if (std::string(path).size() == Const::LTFSDM_LOCK_DIR.size()) {
            statbuf->st_mode = S_IFDIR | S_IRWXU;
            goto end;
        } else if (path[Const::LTFSDM_LOCK_DIR.size()] == '/') {
            statbuf->st_mode = S_IFREG | S_IRWXU;
            statbuf->st_ino = strtoul(
                    path + (Const::LTFSDM_LOCK_DIR.size() + 1), 0, 10);
            goto end;
        }
    }

    if (fstatat(getshrd()->rootFd, FuseFS::relPath(path), statbuf,
    AT_SYMLINK_NOFOLLOW) == -1) {
        if (Const::LTFSDM_IOCTL.compare(path) == 0) {
            return (-1 * EACCES);
        } else {
            return (-1 * errno);
        }
    } else {
        if ((fd = openat(getshrd()->rootFd, FuseFS::relPath(path), O_RDONLY))
                == -1)
            goto end;
        try {
            miginfo = getMigInfoAt(fd);
        } catch (const std::exception& e) {
            MSG(LTFSDMF0057E, path);
            close(fd);
            goto end;
        }
        if (FuseFS::needsRecovery(miginfo) == true)
            FuseFS::recoverState(path, miginfo.state);
        close(fd);
        if (miginfo.state != FuseFS::mig_state_attr_t::state_num::RESIDENT) {
            statbuf->st_size = miginfo.size;
            statbuf->st_atim = miginfo.atime;
            statbuf->st_mtim = miginfo.mtime;
        }
        goto end;
    }

    end:
    TRACE(Trace::always, pid, path);
    return 0;
}

int FuseFS::ltfsdm_access(const char *path, int mask)

{
    if (faccessat(getshrd()->rootFd, FuseFS::relPath(path), mask, 0) == -1)
        return (-1 * errno);
    else
        return 0;
}

int FuseFS::ltfsdm_readlink(const char *path, char *buffer, size_t size)

{
    memset(buffer, 0, size);

    if (readlinkat(getshrd()->rootFd, FuseFS::relPath(path), buffer, size - 1)
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

    if ((fd = openat(getshrd()->rootFd, FuseFS::relPath(path), O_RDONLY)) == -1)
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
    FuseFS::mig_state_attr_t miginfo;
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
            try {
                miginfo = getMigInfoAt(fd);
            } catch (const LTFSDMException &e) {
                TRACE(Trace::error, e.what());
                MSG(LTFSDMF0057E, path);
                if (e.getError() != Error::ATTR_FORMAT) {
                    close(fd);
                    return (-1 * EIO);
                } else {
                    memset(&miginfo, 0, sizeof(miginfo));
                }
            } catch (const std::exception& e) {
                TRACE(Trace::error, e.what());
                MSG(LTFSDMF0057E, path);
                close(fd);
                return (-1 * EIO);
            }
            close(fd);
            if (miginfo.state != FuseFS::mig_state_attr_t::state_num::RESIDENT
                    && miginfo.state
                            != FuseFS::mig_state_attr_t::state_num::IN_MIGRATION)
                statbuf.st_size = miginfo.size;
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

    if (mknodat(getshrd()->rootFd, FuseFS::relPath(path), mode, rdev) == -1) {
        return (-1 * errno);
    } else {
        fc = fuse_get_context();
        if (fchownat(getshrd()->rootFd, FuseFS::relPath(path), fc->uid, fc->gid,
        AT_SYMLINK_NOFOLLOW) == -1)
            return (-1 * errno);
    }
    return 0;
}

int FuseFS::ltfsdm_mkdir(const char *path, mode_t mode)

{
    struct fuse_context *fc = fuse_get_context();

    if (mkdirat(getshrd()->rootFd, FuseFS::relPath(path), mode) == -1) {
        return (-1 * errno);
    } else {
        fc = fuse_get_context();
        if (fchownat(getshrd()->rootFd, FuseFS::relPath(path), fc->uid, fc->gid,
        AT_EMPTY_PATH | AT_SYMLINK_NOFOLLOW) == -1)
            return (-1 * errno);
    }
    return 0;
}

int FuseFS::ltfsdm_unlink(const char *path)

{
    if (unlinkat(getshrd()->rootFd, FuseFS::relPath(path), 0) == -1)
        return (-1 * errno);
    else
        return 0;
}

int FuseFS::ltfsdm_rmdir(const char *path)

{
    if (unlinkat(getshrd()->rootFd, FuseFS::relPath(path), AT_REMOVEDIR) == -1)
        return (-1 * errno);
    else
        return 0;
}

int FuseFS::ltfsdm_symlink(const char *target, const char *linkpath)

{
    if (symlinkat(target, getshrd()->rootFd, FuseFS::relPath(linkpath)) == -1)
        return (-1 * errno);
    else
        return 0;
}

int FuseFS::ltfsdm_rename(const char *oldpath, const char *newpath)

{
    if (renameat(getshrd()->rootFd, FuseFS::relPath(oldpath), getshrd()->rootFd,
            FuseFS::relPath(newpath)) == -1)
        return (-1 * errno);
    else
        return 0;
}

int FuseFS::ltfsdm_link(const char *oldpath, const char *newpath)

{
    if (linkat(getshrd()->rootFd, FuseFS::relPath(oldpath), getshrd()->rootFd,
            FuseFS::relPath(newpath), AT_EMPTY_PATH) == -1)
        return (-1 * errno);
    else
        return 0;
}

int FuseFS::ltfsdm_chmod(const char *path, mode_t mode)

{
    if (fchmodat(getshrd()->rootFd, FuseFS::relPath(path), mode, 0) == -1)
        return (-1 * errno);
    else
        return 0;
}

int FuseFS::ltfsdm_chown(const char *path, uid_t uid, gid_t gid)

{
    if (fchownat(getshrd()->rootFd, FuseFS::relPath(path), uid, gid,
    AT_SYMLINK_NOFOLLOW) == -1)
        return (-1 * errno);
    else
        return 0;
}

int FuseFS::ltfsdm_truncate(const char *path, off_t size)

{
    FuseFS::mig_state_attr_t migInfo;
    ssize_t attrsize;
    FuseFS::ltfsdm_file_info linfo = (FuseFS::ltfsdm_file_info ) { 0, 0, "" };

    linfo.fusepath = path;

    if ((linfo.fd = openat(getshrd()->rootFd, FuseFS::relPath(path), O_WRONLY))
            == -1) {
        TRACE(Trace::error, errno);
        return (-1 * errno);
    }

    memset(&migInfo, 0, sizeof(FuseFS::mig_state_attr_t));

    try {
        FuseLock main_lock(FuseFS::lockPath(path), FuseLock::main,
                FuseLock::lockshared);
        std::unique_lock<FuseLock> mainlock(main_lock);

        FuseLock trec_lock(FuseFS::lockPath(path), FuseLock::fuse,
                FuseLock::lockexclusive);

        std::lock_guard<FuseLock> treclock(trec_lock);

        if ((attrsize = fgetxattr(linfo.fd, Const::LTFSDM_EA_MIGSTATE.c_str(),
                (void *) &migInfo, sizeof(migInfo))) == -1) {
            if ( errno != ENODATA) {
                close(linfo.fd);
                TRACE(Trace::error, fuse_get_context()->pid);
                return (-1 * errno);
            }
        }

        if ((size > 0)
                && ((migInfo.state == FuseFS::mig_state_attr_t::state_num::MIGRATED)
                        || (migInfo.state
                                == FuseFS::mig_state_attr_t::state_num::IN_RECALL))) {
            TRACE(Trace::full, path);
            mainlock.unlock();
            if (recall_file(&linfo, true) == -1) {
                close(linfo.fd);
                return (-1 * EIO);
            }
            mainlock.lock();
        } else if (attrsize != -1) {
            if (fremovexattr(linfo.fd, Const::LTFSDM_EA_MIGSTATE.c_str())
                    == -1) {
                close(linfo.fd);
                return (-1 * EIO);
            }
            if (fremovexattr(linfo.fd, Const::LTFSDM_EA_MIGINFO.c_str())
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
    if (utimensat(getshrd()->rootFd, FuseFS::relPath(path), times,
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

    if (getshrd()->rootFd == Const::UNSET
            && Const::LTFSDM_IOCTL.compare(path) == 0) {
        linfo = new (FuseFS::ltfsdm_file_info);
        linfo->fd = Const::UNSET;
        linfo->lfd = Const::UNSET;
        linfo->fusepath = path;
        linfo->main_lock = nullptr;
        linfo->trec_lock = nullptr;
        finfo->fh = (unsigned long) linfo;
        return 0;
    }

    if (std::string(path).compare(0, Const::LTFSDM_LOCK_DIR.size() + 1,
            Const::LTFSDM_LOCK_DIR + "/") == 0) {
        TRACE(Trace::always, path);
        linfo = new (FuseFS::ltfsdm_file_info);
        linfo->fd = Const::UNSET;
        linfo->lfd = Const::UNSET;
        linfo->fusepath = path;
        linfo->main_lock = nullptr;
        linfo->trec_lock = nullptr;
        finfo->fh = (unsigned long) linfo;
        return 0;
    }

    if ((fd = openat(getshrd()->rootFd, FuseFS::relPath(path), finfo->flags))
            == -1) {
        TRACE(Trace::error, fuse_get_context()->pid);
        return (-1 * errno);
    }

    linfo = new (FuseFS::ltfsdm_file_info);

    linfo->fd = fd;
    linfo->lfd = lfd;
    linfo->fusepath = path;
    linfo->main_lock = nullptr;
    linfo->trec_lock = nullptr;

    try {
        linfo->main_lock = new FuseLock(FuseFS::lockPath(path), FuseLock::main,
                FuseLock::lockshared);
    } catch (const std::exception& e) {
        TRACE(Trace::error, FuseFS::lockPath(path));
        return (-1 * EACCES);
    }

    try {
        linfo->trec_lock = new FuseLock(FuseFS::lockPath(path), FuseLock::fuse,
                FuseLock::lockexclusive);
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
    FuseFS::mig_state_attr_t migInfo;
    ssize_t attrsize;
    FuseFS::ltfsdm_file_info *linfo = (FuseFS::ltfsdm_file_info *) finfo->fh;

    if (linfo == NULL) {
        TRACE(Trace::error, fuse_get_context()->pid);
        return (-1 * EBADF);
    }

    if (FuseFS::procIsLTFSDM(fuse_get_context()->pid) == false) {
        memset(&migInfo, 0, sizeof(FuseFS::mig_state_attr_t));

        std::unique_lock<FuseLock> mainlock(*(linfo->main_lock));

        try {
            std::lock_guard<FuseLock> treclock(*(linfo->trec_lock));

            if ((attrsize = fgetxattr(linfo->fd,
                    Const::LTFSDM_EA_MIGSTATE.c_str(), (void *) &migInfo,
                    sizeof(migInfo))) == -1) {
                if ( errno != ENODATA) {
                    TRACE(Trace::error, fuse_get_context()->pid);
                    return (-1 * errno);
                }
            }

            if ((size > 0)
                    && ((migInfo.state == FuseFS::mig_state_attr_t::state_num::MIGRATED)
                            || (migInfo.state
                                    == FuseFS::mig_state_attr_t::state_num::IN_RECALL))) {
                TRACE(Trace::full, linfo->fd);
                mainlock.unlock();
                if (recall_file(linfo, true) == -1) {
                    return (-1 * EIO);
                }
                mainlock.lock();
            } else if (attrsize != -1) {
                if (fremovexattr(linfo->fd, Const::LTFSDM_EA_MIGSTATE.c_str())
                        == -1)
                    return (-1 * EIO);
                if (fremovexattr(linfo->fd, Const::LTFSDM_EA_MIGINFO.c_str())
                        == -1)
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
    FuseFS::mig_state_attr_t migInfo;
    ssize_t attrsize;
    FuseFS::ltfsdm_file_info *linfo = (FuseFS::ltfsdm_file_info *) finfo->fh;

    assert(path == NULL);

    if (linfo == NULL) {
        TRACE(Trace::error, fuse_get_context()->pid);
        return (-1 * EBADF);
    }

    memset(&migInfo, 0, sizeof(FuseFS::mig_state_attr_t));

    std::unique_lock<FuseLock> mainlock(*(linfo->main_lock));

    TRACE(Trace::always, linfo->fd);

    try {
        std::lock_guard<FuseLock> treclock(*(linfo->trec_lock));

        if ((attrsize = fgetxattr(linfo->fd, Const::LTFSDM_EA_MIGSTATE.c_str(),
                (void *) &migInfo, sizeof(migInfo))) == -1) {
            if ( errno != ENODATA) {
                TRACE(Trace::error, fuse_get_context()->pid, errno);
                return (-1 * errno);
            }
        }

        if (migInfo.state == FuseFS::mig_state_attr_t::state_num::MIGRATED
                || migInfo.state == FuseFS::mig_state_attr_t::state_num::IN_RECALL) {
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
    FuseFS::mig_state_attr_t migInfo;
    ssize_t attrsize;
    FuseFS::ltfsdm_file_info *linfo = (FuseFS::ltfsdm_file_info *) finfo->fh;

    struct fuse_bufvec dest = FUSE_BUFVEC_INIT(fuse_buf_size(buf));

    assert(path == NULL);

    if (linfo == NULL)
        return (-1 * EBADF);

    memset(&migInfo, 0, sizeof(FuseFS::mig_state_attr_t));

    std::unique_lock<FuseLock> mainlock(*(linfo->main_lock));

    try {
        std::lock_guard<FuseLock> treclock(*(linfo->trec_lock));

        if ((attrsize = fgetxattr(linfo->fd, Const::LTFSDM_EA_MIGSTATE.c_str(),
                (void *) &migInfo, sizeof(migInfo))) == -1) {
            if ( errno != ENODATA) {
                TRACE(Trace::error, errno);
                return (-1 * errno);
            }
        }

        if (migInfo.state == FuseFS::mig_state_attr_t::state_num::MIGRATED
                || migInfo.state == FuseFS::mig_state_attr_t::state_num::IN_RECALL) {
            TRACE(Trace::full, linfo->fd);
            mainlock.unlock();
            if (recall_file(linfo, true) == -1) {
                return (-1 * EIO);
            }
            mainlock.lock();
        } else if (migInfo.state == FuseFS::mig_state_attr_t::state_num::PREMIGRATED) {
            if (fremovexattr(linfo->fd, Const::LTFSDM_EA_MIGSTATE.c_str())
                    == -1) {
                TRACE(Trace::error, errno);
                return (-1 * EIO);
            }
            if (fremovexattr(linfo->fd, Const::LTFSDM_EA_MIGINFO.c_str())
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

    if ((fd = openat(getshrd()->rootFd, FuseFS::relPath(path), O_RDONLY)) == -1)
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

    if (getshrd()->rootFd == Const::UNSET
            && Const::LTFSDM_IOCTL.compare(linfo->fusepath) == 0) {
        TRACE(Trace::always, linfo->fusepath);
        delete (linfo);
        return 0;
    }

    if (linfo->main_lock != nullptr)
        delete (linfo->main_lock);

    if (linfo->trec_lock != nullptr)
        delete (linfo->trec_lock);

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

    if (FuseFS::procIsLTFSDM(pid) == false
            && std::string(name).find(Const::LTFSDM_EA.c_str())
                    != std::string::npos)
        return (-1 * EPERM);

    if ((fd = openat(getshrd()->rootFd, FuseFS::relPath(path), O_RDONLY)) == -1)
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

    if (Const::LTFSDM_EA_FSINFO.compare(name) == 0) {
        assert(size == sizeof(fh));
        memset(&fh, 0, sizeof(fh));
        strncpy(fh.mountpoint, getshrd()->mountpt.c_str(), PATH_MAX - 1);
        strncpy(fh.fusepath, relPath(path), PATH_MAX - 1);
        strncpy(fh.lockpath, FuseFS::lockPath(path).c_str(), PATH_MAX - 1);
        fh.fsid_h = getshrd()->fsid_h;
        fh.fsid_l = getshrd()->fsid_l;
        fh.fd = Const::UNSET;
        *(FuseFS::FuseHandle *) value = fh;
        TRACE(Trace::always, fh.fusepath, sizeof(fh));
        return size;
    }

    if (Const::LTFSDM_CACHE_DIR.compare(path) == 0)
        return (-1 * ENODATA);

    if ((fd = openat(getshrd()->rootFd, FuseFS::relPath(path), O_RDONLY)) == -1)
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

    if ((fd = openat(getshrd()->rootFd, FuseFS::relPath(path), O_RDONLY)) == -1)
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

    if (std::string(name).find(Const::LTFSDM_EA.c_str()) != std::string::npos)
        return (-1 * ENOTSUP);

    if ((fd = openat(getshrd()->rootFd, FuseFS::relPath(path), O_RDONLY)) == -1)
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
            strncpy(fh.mountpoint, getshrd()->mountpt.c_str(), PATH_MAX - 1);
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
            setRootFd(open(getshrd()->srcdir.c_str(), O_RDONLY));
            TRACE(Trace::always, getshrd()->rootFd, errno);
            if (getshrd()->rootFd == -1)
                return (-1 * errno);
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

void FuseFS::ltfsdm_destroy(void *ptr)

{
    MSG(LTFSDMF0063I);
    if (getshrd()->rootFd != Const::UNSET) {
        close(getshrd()->rootFd);
        setRootFd(Const::UNSET);
    }
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
    ops.destroy = FuseFS::ltfsdm_destroy;

    return ops;
}

void FuseFS::execute(std::string sourcedir, std::string mountpt,
        std::string command)

{
    pthread_setname_np(pthread_self(), "ltfsdmd.ofs");
    int ret;
    char c;
    std::string line;
    FILE *cmd;

    if ((cmd = popen(command.c_str(), "re")) == NULL) {
        MSG(LTFSDMF0058E, mountpt);
        return;
    }

    while (!feof(cmd)) {
        if ((c = fgetc(cmd)) != 0) {
            if (c == 0012) {
                MSG(LTFSDMF0060I, mountpt, line);
                line.clear();
            } else {
                if (c != EOF)
                    line += c;
            }
        }
    }

    if (line.size() > 0)
        MSG(LTFSDMF0060I, mountpt, line);

    ret = pclose(cmd);

    try {
        FileSystems fss;

        if (!WIFEXITED(ret) || WEXITSTATUS(ret)) {
            TRACE(Trace::error, ret, WIFEXITED(ret), WEXITSTATUS(ret));
            MSG(LTFSDMF0023E, sourcedir, WEXITSTATUS(ret));
            fss.umount(mountpt, FileSystems::UMNT_DETACHED_FORCED);
        } else if (Connector::connectorTerminate == false) {
            MSG(LTFSDMF0030I, sourcedir);
            fss.umount(mountpt, FileSystems::UMNT_DETACHED_FORCED);
        }
    } catch (const std::exception& e) {
        TRACE(Trace::error, e.what());
    }
}

std::string FuseFS::mask(std::string s)

{
    std::string masked;

    for (char c : s) {
        switch (c) {
            case '\\':
            case '\"':
            case '\'':
            case ' ':
            case '`':
            case '&':
            case '(':
            case ')':
            case '*':
            case ';':
            case '<':
            case '>':
            case '?':
            case '$':
            case '|':
                masked += '\\';
            default:
                masked += c;
        }
    }

    return masked;
}

/**
    @brief Setup of LTFS Data Management for a file system.
    @details
    Setup of LTFS Data Management for a file system.

    The setup happens according the following steps:
    -# Unmount the original file system.
    -# Perform a so called fake mount (see <TT>mount -f</TT> command) by only
       specifying the mount point to see if it can be mounted automatically.
       Any file system managed by LTFS Data Management should not be mounted
       beside the LTFS Data Management service (especially not automatically
       during system startup).
    -# Start of the Fuse overlay file system. The Fuse overlay file system
       is mounted at the original mount point.
    -# Wait for the Fuse overlay file system to be  in operation and open a
       file descriptor for the ioctl communication.
    -# Mount the original file system within the cache mount point
       Const::LTFSDM_CACHE_MP.
    -# Open the file descriptor FuseFS::rootFd on its root: i.e.
       @<original mount point@>/Const@::LTFSDM_CACHE_MP.
    -# Via ioctl tell the Fuse process to continue. It was blocked before
       since it can only be fully operational if access to the orginal
       file system is available.
    -# Perform a detached unmount of the original file system.

    After the last step there is no general access possible to the original
    file system. However the LTFS Data Management service is able to access
    the file system via FuseFS::rootFd file descriptor. The procedure listed
    here i.a. is to guarantee that the original file system is not in use
    anymore when doing the management. If one of the steps fails the original
    file system will not be managed.

    @param starttime start time of the LTFS Data Management service
 */
void FuseFS::init(struct timespec starttime)

{
    std::stringstream stream;
    char exepath[PATH_MAX];
    int fd = Const::UNSET;
    int count = 0;
    FileSystems fss;
    FileSystems::fsinfo fs;
    bool alreadyManaged = false;

    if (Connector::conf == nullptr)
        THROW(Error::GENERAL_ERROR);

    memset(exepath, 0, PATH_MAX);
    if (readlink("/proc/self/exe", exepath, PATH_MAX - 1) == -1) {
        MSG(LTFSDMC0021E);
        THROW(Error::GENERAL_ERROR);
    }

    FsObj target(mountpt);

    if (target.isFsManaged()) {
        FileSystems::fsinfo fschk;
        alreadyManaged = true;
        fs = Connector::conf->getFs(mountpt);
        try {
            fschk = fss.getByTarget(mountpt);
            MSG(LTFSDMF0061E, mountpt);
            try {
                fss.umount(mountpt, FileSystems::UMNT_NORMAL);
            } catch (const std::exception& e) {
                TRACE(Trace::error, e.what());
                MSG(LTFSDMF0039E, mountpt);
                THROW(Error::FS_UNMOUNT);
            }
        } catch (const LTFSDMException& e) {
            if (e.getError() == Error::FS_UNMOUNT)
                THROW(Error::GENERAL_ERROR);
            TRACE(Trace::always, e.what());
        }
    } else {
        try {
            fs = fss.getByTarget(mountpt);
        } catch (const std::exception& e) {
            TRACE(Trace::error, e.what());
            MSG(LTFSDMS0080E, mountpt);
            THROW(Error::GENERAL_ERROR);
        }

        MSG(LTFSDMF0038I, mountpt);

        try {
            fss.umount(mountpt, FileSystems::UMNT_NORMAL);
        } catch (const std::exception& e) {
            TRACE(Trace::error, e.what());
            MSG(LTFSDMF0039E, mountpt);
            THROW(Error::GENERAL_ERROR);
        }
    }

    try {
        fss.mount("", mountpt, "", FileSystems::MNT_FAKE);
        MSG(LTFSDMF0062E, mountpt);
        THROW(Error::FS_IN_FSTAB);
    } catch (const LTFSDMException& e) {
        if (e.getError() == Error::FS_IN_FSTAB)
            THROW(Error::GENERAL_ERROR);
    }

    stream << mask(dirname(exepath)) << "/" << Const::OVERLAY_FS_COMMAND
            << " -m " << mask(mountpt) << " -f " << mask(fs.source) << " -S "
            << starttime.tv_sec << " -N " << starttime.tv_nsec << " -l "
            << messageObject.getLogType() << " -t " << traceObject.getTrclevel()
            << " -p " << getpid() << " 2>&1";
    TRACE(Trace::always, stream.str());
    thrd = new std::thread(&FuseFS::execute, (mountpt + Const::LTFSDM_CACHE_MP),
            mountpt, stream.str());

    while (true) {
        if ((fd = open((mountpt + Const::LTFSDM_IOCTL).c_str(),
        O_RDONLY | O_CLOEXEC)) == -1) {
            if (count < 20) {
                MSG(LTFSDMF0041I);
                sleep(1);
                count++;
                continue;
            }
            thrd->join();
            delete (thrd);
            MSG(LTFSDMF0040E, errno);
            THROW(Error::GENERAL_ERROR);
        } else if (ioctl(fd, FuseFS::LTFSDM_PREMOUNT) != -1) {
            break;
        } else if (count < 20) {
            MSG(LTFSDMF0041I);
            close(fd);
            sleep(1);
            count++;
        } else {
            close(fd);
            thrd->join();
            delete (thrd);
            MSG(LTFSDMF0040E, errno);
            THROW(Error::GENERAL_ERROR);
        }
    }

    init_status.FUSE_STARTED = true;
    FuseFS::ioctlFd = fd;

    MSG(LTFSDMF0036I, fs.source, mountpt + Const::LTFSDM_CACHE_MP);

    try {
        fss.mount(fs.source, mountpt + Const::LTFSDM_CACHE_MP, fs.options,
                FileSystems::MNT_NORMAL);
    } catch (const std::exception &e) {
        TRACE(Trace::error, e.what());
        MSG(LTFSDMF0037E, fs.source, e.what());
        THROW(Error::GENERAL_ERROR);
    }
    init_status.CACHE_MOUNTED = true;

    MSG(LTFSDMF0046I, mountpt + Const::LTFSDM_CACHE_MP);
    if ((rootFd = open((mountpt + Const::LTFSDM_CACHE_MP).c_str(),
    O_RDONLY | O_CLOEXEC)) == -1) {
        MSG(LTFSDMF0047E, errno);
        THROW(Error::GENERAL_ERROR);
    }

    MSG(LTFSDMF0048I, mountpt);
    if (ioctl(fd, FuseFS::LTFSDM_POSTMOUNT) == -1) {
        MSG(LTFSDMF0049E, errno);
        THROW(Error::GENERAL_ERROR);
    }
    init_status.ROOTFD_FUSE = true;

    MSG(LTFSDMF0050I, mountpt);
    try {
        fss.umount(mountpt + Const::LTFSDM_CACHE_MP,
                FileSystems::UMNT_DETACHED);
    } catch (const std::exception& e) {
        TRACE(Trace::error, e.what());
        MSG(LTFSDMF0051E, mountpt, errno);
        THROW(Error::GENERAL_ERROR);
    }
    init_status.CACHE_MOUNTED = false;

    if (alreadyManaged == false)
        Connector::conf->addFs(fs);
}

FuseFS::~FuseFS()

{
    try {
        FileSystems fss;

        if (rootFd != Const::UNSET)
            close(rootFd);

        if (FuseFS::ioctlFd != Const::UNSET)
            close(FuseFS::ioctlFd);

        if (init_status.CACHE_MOUNTED) {
            MSG(LTFSDMF0054I, Const::LTFSDM_CACHE_MP);
            try {
                fss.umount(mountpt + Const::LTFSDM_CACHE_MP,
                        FileSystems::UMNT_NORMAL);
            } catch (const std::exception& e) {
                TRACE(Trace::error, e.what());
                MSG(LTFSDMF0053E, mountpt + Const::LTFSDM_CACHE_MP, e.what());
            }
        }

        if (init_status.FUSE_STARTED == false)
            return;

        MSG(LTFSDMF0028I, mountpt.c_str());
        MSG(LTFSDMF0007I);
        do {
            if (Connector::forcedTerminate)
                fss.umount(mountpt, FileSystems::UMNT_DETACHED_FORCED);
            else {
                try {
                    fss.umount(mountpt, FileSystems::UMNT_NORMAL);
                } catch (const LTFSDMException& e) {
                    if (e.getError() == Error::FS_BUSY) {
                        MSG(LTFSDMF0003I, mountpt);
                        sleep(1);
                        continue;
                    } else {
                        fss.umount(mountpt, FileSystems::UMNT_DETACHED_FORCED);
                    }
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
