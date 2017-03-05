#include <stdio.h> // for rename
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <linux/fs.h>
#include <dirent.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/xattr.h>
#include <sys/ioctl.h>
#include <assert.h>
#include <errno.h>

#define FUSE_USE_VERSION 26

#include <fuse.h>

#include <string>
#include <sstream>
#include <set>
#include <atomic>
#include <condition_variable>
#include <thread>
#include <mutex>

#include "src/common/util/util.h"
#include "src/common/messages/Message.h"
#include "src/common/tracing/Trace.h"
#include "src/common/errors/errors.h"
#include "src/common/const/Const.h"

#include "src/connector/Connector.h"
#include "FuseFS.h"

thread_local std::string lsourcedir = "";
std::mutex trecall_mtx;
std::condition_variable trecall_cond;
std::mutex trecall_reply_mtx;
std::condition_variable trecall_reply_cond;
std::condition_variable trecall_reply_wait_cond;
std::atomic<fuid_t> trecall_fuid;
Connector::rec_info_t recinfo_share;
std::condition_variable wait_cond;
std::atomic<bool> single(false);

struct ltfs_file_info {
	int fd;
	std::string sourcepath;
	std::string fusepath;
};

struct ltfsdm_dir_info {
	DIR *dir;
	struct dirent *dentry;
	off_t offset;
};

mig_info_t FuseFS::genMigInfo(const char *path, mig_info_t::state_t state)

{
	mig_info_t miginfo;

	memset(&miginfo, 0, sizeof(miginfo));

	if ( stat(path, &miginfo.statinfo) ) {
		TRACE(Trace::error, errno);
		throw(errno);
	}

	miginfo.state = state;

	clock_gettime(CLOCK_REALTIME, &miginfo.changed);

	return miginfo;
}


void FuseFS::setMigInfo(const char *path, mig_info_t::state_t state)

{
	ssize_t size;
	mig_info_t miginfo_new;
	mig_info_t miginfo;

	miginfo_new = genMigInfo(path, state);

	memset(&miginfo, 0, sizeof(miginfo));

	if ( (size = getxattr(path, Const::OPEN_LTFS_EA_MIGINFO_INT.c_str(), (void *) &miginfo, sizeof(miginfo))) == -1 ) {
		if ( errno != ENODATA ) {
			throw(errno);
		}
	}
	else if ( size != sizeof(miginfo) ) {
		throw(EIO);
	}

	if ( miginfo.state != mig_info_t::state_t::NO_STATE )
		miginfo_new.statinfo.st_size = miginfo.statinfo.st_size;

	if ( setxattr(path, Const::OPEN_LTFS_EA_MIGINFO_INT.c_str(), (void *) &miginfo_new, sizeof(miginfo_new), 0) == -1 )
		throw(errno);
}


void FuseFS::remMigInfo(const char *path)

{
	if ( removexattr(path, Const::OPEN_LTFS_EA_MIGINFO_INT.c_str()) == -1 )
		throw(errno);
}


mig_info_t FuseFS::getMigInfo(const char *path)

{
	ssize_t size;
	mig_info_t miginfo;

	memset(&miginfo, 0, sizeof(miginfo));

	if ( (size = getxattr(path, Const::OPEN_LTFS_EA_MIGINFO_INT.c_str(), (void *) &miginfo, sizeof(miginfo))) == -1 ) {
		// TODO
		/* check for errno */
		return miginfo;
	}
	else if ( size != sizeof(miginfo) ) {
		throw(EIO);
	}

	return miginfo;
}


mig_info_t FuseFS::getMigInfoAt(int dirfd, const char *path)

{
	ssize_t size;
	mig_info_t miginfo;
	int fd;

	memset(&miginfo, 0, sizeof(miginfo));

	if ( (fd = openat(dirfd, path, O_RDONLY)) == -1 )
		throw(errno);

	if ( (size = fgetxattr(fd, Const::OPEN_LTFS_EA_MIGINFO_INT.c_str(), (void *) &miginfo, sizeof(miginfo))) == -1 ) {
		// TODO
		/* check for errno */
		close(fd);
		return miginfo;
	}

	close(fd);

	if ( size != sizeof(miginfo) )
		throw(EIO);

	return miginfo;
}



bool FuseFS::needsRecovery(mig_info_t miginfo)

{
	struct fuse_context *fc = fuse_get_context();

	if ((miginfo.state == mig_info_t::state_t::IN_MIGRATION) |
		(miginfo.state == mig_info_t::state_t::STUBBING) |
		(miginfo.state == mig_info_t::state_t::IN_RECALL) ) {

		if ( ((openltfs_ctx_t *) fc->private_data)->starttime.tv_sec < miginfo.changed.tv_sec )
			return false;
		else if ( (((openltfs_ctx_t *) fc->private_data)->starttime.tv_sec == miginfo.changed.tv_sec)  &&
				  (((openltfs_ctx_t *) fc->private_data)->starttime.tv_nsec < miginfo.changed.tv_nsec) )
			return false;
		else
			return true;
	}

	return false;
}

void FuseFS::recoverState(const char *path, mig_info_t::state_t state)

{
	// TODO
}

std::string FuseFS::souce_path(const char *path)

{
	std::string fullpath;
	mig_info_t miginfo;

	struct fuse_context *fc = fuse_get_context();
	fullpath =  ((openltfs_ctx_t *) fc->private_data)->sourcedir + std::string(path);

	try {
		miginfo = getMigInfo(fullpath.c_str());
	}
	catch (int error) {
		TRACE(Trace::error, error);
		MSG(LTFSDMF0011E, fullpath);
		return std::string("");
	}
	if ( FuseFS::needsRecovery(miginfo) == true )
		 FuseFS::recoverState(fullpath.c_str(), miginfo.state);

	return fullpath;
}

int FuseFS::ltfsdm_getattr(const char *path, struct stat *statbuf)

{
	mig_info_t miginfo;

	memset(statbuf, 0, sizeof(struct stat));

	if ( lstat(FuseFS::souce_path(path).c_str(), statbuf) == -1 ) {
		return (-1*errno);
	}
	else {
		miginfo = getMigInfo(FuseFS::souce_path(path).c_str());
		if ( miginfo.state != mig_info_t::state_t::NO_STATE )
			statbuf->st_size = miginfo.statinfo.st_size;
		return 0;
	}
}

int FuseFS::ltfsdm_access(const char *path, int mask)

{
	if ( access(FuseFS::souce_path(path).c_str(), mask) == -1 )
		return (-1*errno);
	else
		return 0;
}

int FuseFS::ltfsdm_readlink(const char *path, char *buffer, size_t size)

{
	memset(buffer, 0, size);

	if ( readlink(FuseFS::souce_path(path).c_str(), buffer, size - 1) == -1 )
		return (-1*errno);
	else
		return 0;
}

int FuseFS::ltfsdm_opendir(const char *path, struct fuse_file_info *finfo)

{
	ltfsdm_dir_info *dirinfo = NULL;
	DIR *dir = NULL;

	if ( (dir = opendir(FuseFS::souce_path(path).c_str())) == 0 ) {
		return (-1*errno);
	}

	dirinfo = new(ltfsdm_dir_info);
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
	mig_info_t miginfo;
	off_t next;
	ltfsdm_dir_info *dirinfo = (ltfsdm_dir_info *) finfo->fh;

	assert(path == NULL);

	if ( dirinfo == NULL )
		return (-1*EBADF);

	if (offset != dirinfo->offset) {
		seekdir(dirinfo->dir, offset);
		dirinfo->dentry = NULL;
		dirinfo->offset = offset;
	}

	while(true) {
		if ( ! dirinfo->dentry ) {
			if ((dirinfo->dentry = readdir(dirinfo->dir)) == NULL) {
				break;
			}
		}

		if ( fstatat(dirfd(dirinfo->dir), dirinfo->dentry->d_name, &statbuf, AT_SYMLINK_NOFOLLOW) == -1 )
		 	return (-1*errno);

		next = telldir(dirinfo->dir);

		if ( S_ISREG(statbuf.st_mode) ) {
			miginfo = getMigInfoAt(dirfd(dirinfo->dir), dirinfo->dentry->d_name);
			if ( miginfo.state != mig_info_t::state_t::NO_STATE )
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
	ltfsdm_dir_info *dirinfo = (ltfsdm_dir_info *) finfo->fh;

	assert(path == NULL);

	if ( dirinfo == NULL )
		return (-1*EBADF);

	closedir(dirinfo->dir);
	delete(dirinfo);

	return 0;
}

int FuseFS::ltfsdm_mknod(const char *path, mode_t mode, dev_t rdev)

{
	struct fuse_context *fc;

	if ( mknod(FuseFS::souce_path(path).c_str(), mode, rdev) == -1 ) {
		return (-1*errno);
	}
	else {
		fc = fuse_get_context();
		if ( chown(FuseFS::souce_path(path).c_str(), fc->uid, fc->gid) == -1 )
			return (-1*errno);
	}
	return 0;
}

int FuseFS::ltfsdm_mkdir(const char *path, mode_t mode)

{
	struct fuse_context *fc;

	if ( mkdir(FuseFS::souce_path(path).c_str(), mode) == -1 ) {
		return (-1*errno);
	}
	else {
		fc = fuse_get_context();
		if ( chown(FuseFS::souce_path(path).c_str(), fc->uid, fc->gid) == -1 )
			return (-1*errno);
	}
	return 0;
}

int FuseFS::ltfsdm_unlink(const char *path)

{
	if ( unlink(FuseFS::souce_path(path).c_str()) == -1 )
		return (-1*errno);
	else
		return 0;
}

int FuseFS::ltfsdm_rmdir(const char *path)

{
	if ( rmdir(FuseFS::souce_path(path).c_str()) == -1 )
		return (-1*errno);
	else
		return 0;
}

int FuseFS::ltfsdm_symlink(const char *target, const char *linkpath)

{
	if ( symlink(target, FuseFS::souce_path(linkpath).c_str()) == -1 )
		return (-1*errno);
	else
		return 0;
}

int FuseFS::ltfsdm_rename(const char *oldpath, const char *newpath)

{
	if ( rename(FuseFS::souce_path(oldpath).c_str(), FuseFS::souce_path(newpath).c_str()) == -1 )
		return (-1*errno);
	else
		return 0;
}

int FuseFS::ltfsdm_link(const char *oldpath, const char *newpath)

{
	if ( link(FuseFS::souce_path(oldpath).c_str(), FuseFS::souce_path(newpath).c_str()) == -1 )
		return (-1*errno);
	else
		return 0;
}

int FuseFS::ltfsdm_chmod(const char *path, mode_t mode)

{
	if ( chmod(FuseFS::souce_path(path).c_str(), mode) == -1 )
		return (-1*errno);
	else
		return 0;
}

int FuseFS::ltfsdm_chown(const char *path, uid_t uid, gid_t gid)

{
	if ( lchown(FuseFS::souce_path(path).c_str(), uid, gid) == -1 )
		return (-1*errno);
	else
		return 0;
}

int FuseFS::ltfsdm_truncate(const char *path, off_t size)

{
	if ( truncate(FuseFS::souce_path(path).c_str(), size) == -1 )
		return (-1*errno);
	else
		return 0;
}

int FuseFS::ltfsdm_utimens(const char *path, const struct timespec times[2])

{
	if (  utimensat(0, FuseFS::souce_path(path).c_str(), times, AT_SYMLINK_NOFOLLOW) == -1 )
		return (-1*errno);
	else
		return 0;
}

int FuseFS::ltfsdm_open(const char *path, struct fuse_file_info *finfo)

{
	int fd = -1;
	ltfs_file_info *linfo = NULL;

	if ( (fd = open(FuseFS::souce_path(path).c_str(), finfo->flags)) == -1 ) {
		TRACE(Trace::error, fuse_get_context()->pid);
		return (-1*errno);
	}

	linfo = new(ltfs_file_info);
	linfo->fd = fd;
	linfo->fusepath = path;
	linfo->sourcepath = FuseFS::souce_path(path);

	finfo->fh = (unsigned long) linfo;

	return 0;
}

int FuseFS::ltfsdm_read(const char *path, char *buffer, size_t size, off_t offset,
							   struct fuse_file_info *finfo)

{
	ssize_t rsize = -1;
	mig_info_t migInfo;
	ssize_t attrsize;
	ltfs_file_info *linfo = (ltfs_file_info *) finfo->fh;

	assert(path == NULL);

	if ( linfo == NULL ) {
		TRACE(Trace::error, fuse_get_context()->pid);
		return (-1*EBADF);
	}

	if ( (attrsize = fgetxattr(linfo->fd, Const::OPEN_LTFS_EA_MIGINFO_INT.c_str(), (void *) &migInfo, sizeof(migInfo))) == -1 ) {
		if ( errno != ENODATA ) {
			TRACE(Trace::error, fuse_get_context()->pid);
			return (-1*errno);
		}
	}

	if ( (rsize =pread(linfo->fd, buffer, size, offset)) == -1 ) {
		TRACE(Trace::error, fuse_get_context()->pid);
		return (-1*errno);
	}
	else {
		TRACE(Trace::error, fuse_get_context()->pid);
		return rsize;
	}
}

int FuseFS::ltfsdm_read_buf(const char *path, struct fuse_bufvec **bufferp,
								   size_t size, off_t offset, struct fuse_file_info *finfo)

{
    struct fuse_bufvec *source;
	mig_info_t migInfo;
	ssize_t attrsize;

	ltfs_file_info *linfo = (ltfs_file_info *) finfo->fh;

	assert(path == NULL);

	if ( linfo == NULL ) {
		TRACE(Trace::error, fuse_get_context()->pid);
		return (-1*EBADF);
	}

	if ( (attrsize = fgetxattr(linfo->fd, Const::OPEN_LTFS_EA_MIGINFO_INT.c_str(), (void *) &migInfo, sizeof(migInfo))) == -1 ) {
		if ( errno != ENODATA ) {
			TRACE(Trace::error, fuse_get_context()->pid);
			TRACE(Trace::error, errno);
			return (-1*errno);
		}
	}

	if ( migInfo.state == mig_info_t::state_t::MIGRATED ||
		 migInfo.state == mig_info_t::state_t::IN_RECALL ) {
		struct stat statbuf;
		unsigned int igen;
		int fd;

		if ( fstat(linfo->fd, &statbuf) == -1 ) {
			TRACE(Trace::error, fuse_get_context()->pid);
			TRACE(Trace::error, errno);
			return (-1*errno);
		}

		if ( ioctl(linfo->fd, FS_IOC_GETVERSION, &igen) ) {
			TRACE(Trace::error, fuse_get_context()->pid);
			TRACE(Trace::error, errno);
			return (-1*errno);
		}

		std::unique_lock<std::mutex> lock(trecall_mtx);
		wait_cond.wait(lock, [](){ return single != false; });
		single = false;

		recinfo_share.toresident = false;
		recinfo_share.fsid = statbuf.st_dev;
		recinfo_share.igen = igen;
		recinfo_share.ino = statbuf.st_ino;
		recinfo_share.filename = linfo->sourcepath;
		if ( (fd = open(recinfo_share.filename.c_str(), O_WRONLY)) == -1 ) {
			TRACE(Trace::error, fuse_get_context()->pid);
			TRACE(Trace::error, errno);
			return (-1*errno);
		}
		recinfo_share.fd = fd;

		std::unique_lock<std::mutex> lock_reply(trecall_reply_mtx);
		trecall_cond.notify_one();
		lock.unlock();
		fuid_t fuid = (fuid_t) {statbuf.st_dev, igen, statbuf.st_ino};
		do {
			trecall_reply_cond.wait(lock_reply);
		} while (fuid != trecall_fuid);

		trecall_reply_wait_cond.notify_one();
		close(fd);
	}

    if ( (source = (fuse_bufvec*) malloc(sizeof(struct fuse_bufvec))) == NULL )
		return (-1*errno);

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
	mig_info_t migInfo;
	ssize_t attrsize;
	ltfs_file_info *linfo = (ltfs_file_info *) finfo->fh;

	assert(path == NULL);

	if ( linfo == NULL )
		return (-1*EBADF);

	if ( (attrsize = fgetxattr(linfo->fd, Const::OPEN_LTFS_EA_MIGINFO_INT.c_str(), (void *) &migInfo, sizeof(migInfo))) == -1 ) {
		if ( errno != ENODATA ) {
			return (-1*errno);
		}
	}

	if ( (wsize = pwrite(linfo->fd, buf, size, offset)) == -1 )
		return (-1*errno);
	else
		return wsize;
}

int FuseFS::ltfsdm_statfs(const char *path, struct statvfs *stbuf)

{
	if ( statvfs(FuseFS::souce_path(path).c_str(), stbuf) == -1 )
		return (-1*errno);
	else
		return 0;
}

int FuseFS::ltfsdm_release(const char *path, struct fuse_file_info *finfo)

{
	ltfs_file_info *linfo = (ltfs_file_info *) finfo->fh;

	assert(path == NULL);

	if ( linfo == NULL )
		return (-1*EBADF);

	if ( close(linfo->fd) == -1 ) {
		delete(linfo);
		return (-1*errno);
	}
	else {
		delete(linfo);
		return 0;
	}
}

int FuseFS::ltfsdm_flush(const char *path, struct fuse_file_info *finfo)

{
	ltfs_file_info *linfo = (ltfs_file_info *) finfo->fh;

	assert(path == NULL);

	if ( linfo == NULL )
		return (-1*EBADF);

	if ( close(dup(linfo->fd)) == -1 )
		return (-1*errno);
	else
		return 0;
}

int FuseFS::ltfsdm_fsync(const char *path, int isdatasync,
								struct fuse_file_info *finfo)

{
	ltfs_file_info *linfo = (ltfs_file_info *) finfo->fh;

	int rc;

	assert(path == NULL);

	if ( linfo == NULL )
		return (-1*EBADF);

	if ( isdatasync )
	  rc = fdatasync(linfo->fd);
	else
	  rc = fsync(linfo->fd);

	if ( rc == -1 )
		return (-1*errno);
	else
		return 0;
}

int FuseFS::ltfsdm_fallocate(const char *path, int mode,
									off_t offset, off_t length, struct fuse_file_info *finfo)

{
	ltfs_file_info *linfo = (ltfs_file_info *) finfo->fh;

	assert(path == NULL);

	if ( linfo == NULL )
		return (-1*EBADF);

	if ( fallocate(linfo->fd, mode, offset, length) == -1 )
		return (-1*errno);
	else
		return 0;
}


int FuseFS::ltfsdm_setxattr(const char *path, const char *name, const char *value,
								   size_t size, int flags)

{
	if ( std::string(name).find(Const::OPEN_LTFS_EA.c_str()) != std::string::npos )
		return (-1*ENOTSUP);

	if ( lsetxattr(FuseFS::souce_path(path).c_str(), name, value, size, flags) == -1 )
		return (-1*errno);
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

	if ( Const::OPEN_LTFS_EA_FSINFO.compare(name) == 0 ) {
		strncpy(value, FuseFS::souce_path(path).c_str(), PATH_MAX - 1);
		size = strlen(value) + 1;
		return size;
	}
	else if ( (attrsize = lgetxattr(FuseFS::souce_path(path).c_str(), name, value, size)) == -1 ) {
		return (-1*errno);
	}
	else if ( std::string(name).find(Const::OPEN_LTFS_EA.c_str()) != std::string::npos ) {
		return (-1*ENOTSUP);
	}
	else {
		return attrsize;
	}
}

int FuseFS::ltfsdm_listxattr(const char *path, char *list, size_t size)

{
	ssize_t attrsize;

	if ( (attrsize = llistxattr(FuseFS::souce_path(path).c_str(), list, size)) == -1 )
		return (-1*errno);
	else
		return attrsize;
}

int FuseFS::ltfsdm_removexattr(const char *path, const char *name)

{
	if ( std::string(name).find(Const::OPEN_LTFS_EA.c_str()) != std::string::npos )
		return (-1*ENOTSUP);

	if ( lremovexattr(FuseFS::souce_path(path).c_str(), name) == -1 )
		return (-1*errno);
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
	struct fuse_operations ltfsdm_operations;

	memset(&ltfsdm_operations, 0, sizeof(ltfsdm_operations));

    ltfsdm_operations.init       	= FuseFS::ltfsdm_init;

	ltfsdm_operations.getattr		= FuseFS::ltfsdm_getattr;
	ltfsdm_operations.access		= FuseFS::ltfsdm_access;
	ltfsdm_operations.readlink		= FuseFS::ltfsdm_readlink;

	ltfsdm_operations.opendir		= FuseFS::ltfsdm_opendir;
	ltfsdm_operations.readdir		= FuseFS::ltfsdm_readdir;
	ltfsdm_operations.releasedir	= FuseFS::ltfsdm_releasedir;

	ltfsdm_operations.mknod			= FuseFS::ltfsdm_mknod;
	ltfsdm_operations.mkdir			= FuseFS::ltfsdm_mkdir;
	ltfsdm_operations.symlink		= FuseFS::ltfsdm_symlink;
	ltfsdm_operations.unlink		= FuseFS::ltfsdm_unlink;
	ltfsdm_operations.rmdir			= FuseFS::ltfsdm_rmdir;
	ltfsdm_operations.rename		= FuseFS::ltfsdm_rename;
	ltfsdm_operations.link			= FuseFS::ltfsdm_link;
	ltfsdm_operations.chmod			= FuseFS::ltfsdm_chmod;
	ltfsdm_operations.chown			= FuseFS::ltfsdm_chown;
	ltfsdm_operations.truncate		= FuseFS::ltfsdm_truncate;
	ltfsdm_operations.utimens		= FuseFS::ltfsdm_utimens;
	ltfsdm_operations.open			= FuseFS::ltfsdm_open;
	ltfsdm_operations.read			= FuseFS::ltfsdm_read;
	ltfsdm_operations.read_buf      = FuseFS::ltfsdm_read_buf;
	ltfsdm_operations.write			= FuseFS::ltfsdm_write;
	ltfsdm_operations.statfs		= FuseFS::ltfsdm_statfs;
	ltfsdm_operations.release		= FuseFS::ltfsdm_release;
	ltfsdm_operations.flush         = FuseFS::ltfsdm_flush;
	ltfsdm_operations.fsync			= FuseFS::ltfsdm_fsync;
	ltfsdm_operations.fallocate		= FuseFS::ltfsdm_fallocate;
	ltfsdm_operations.setxattr		= FuseFS::ltfsdm_setxattr;
	ltfsdm_operations.getxattr		= FuseFS::ltfsdm_getxattr;
	ltfsdm_operations.listxattr		= FuseFS::ltfsdm_listxattr;
	ltfsdm_operations.removexattr	= FuseFS::ltfsdm_removexattr;

	return ltfsdm_operations;
};

FuseFS::FuseFS(std::string sourcedir, std::string mountpt, std::string fsName, struct timespec starttime)
	: mountpt(mountpt)

{
	std::stringstream options;
	struct fuse_args fargs = FUSE_ARGS_INIT(0, NULL);
	struct fuse_operations ltfsdm_operations = init_operations();

	ctx = (struct openltfs_ctx_t *) malloc(sizeof(struct openltfs_ctx_t));
	memset(ctx, 0, sizeof(struct openltfs_ctx_t));
	strncpy(ctx->sourcedir,sourcedir.c_str(), PATH_MAX - 1);
	strncpy(ctx->mountpoint, mountpt.c_str(), PATH_MAX - 1);
	ctx->starttime = starttime;

	MSG(LTFSDMF0001I, ctx->sourcedir, mountpt);

	fuse_opt_add_arg(&fargs, mountpt.c_str());

	options << "-ouse_ino,fsname=OpenLTFS:" << fsName << ",nopath,default_permissions,allow_other,max_background=32768";

	fuse_opt_add_arg(&fargs, options.str().c_str());
	//fuse_opt_add_arg(&fargs, "-d");
	fuse_opt_parse(&fargs, NULL, NULL, NULL);

	if ( fuse_parse_cmdline(&fargs, NULL, NULL, NULL) != 0 ) {
		MSG(LTFSDMF0004E, errno);
		throw(Error::LTFSDM_FS_ADD_ERROR);
	}

	MSG(LTFSDMF0002I, mountpt.c_str());

	openltfsch = fuse_mount(mountpt.c_str(), &fargs);

	if ( openltfsch == NULL ) {
		MSG(LTFSDMF0005E, mountpt.c_str());
		throw(Error::LTFSDM_FS_ADD_ERROR);
	}


	MSG(LTFSDMF0003I);

	openltfs = fuse_new(openltfsch, &fargs,  &ltfsdm_operations, sizeof(ltfsdm_operations), (void *) ctx);

	if ( openltfs == NULL ) {
		MSG(LTFSDMF0006E);
		throw(Error::LTFSDM_FS_ADD_ERROR);
	}

	fusefs = new std::thread(fuse_loop_mt, openltfs);

	std::stringstream threadName;
	threadName << "FS:"  << ctx->sourcedir;
	pthread_setname_np(fusefs->native_handle(), threadName.str().substr(0,14).c_str());


}


FuseFS::~FuseFS()

{
	MSG(LTFSDMF0007I);
	fuse_exit(openltfs);
	fuse_unmount(mountpt.c_str(), openltfsch);
	fusefs->join();
	free(ctx);
}
