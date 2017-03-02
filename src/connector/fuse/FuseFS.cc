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
Connector::rec_info_t recinfo_share;

mig_info_t genMigInfo(const char *path, mig_info_t::state_t state)

{
	mig_info_t miginfo;

	if ( stat(path, &miginfo.statinfo) ) {
		TRACE(Trace::error, errno);
		throw(errno);
	}

	miginfo.state = state;

	clock_gettime(CLOCK_REALTIME, &miginfo.changed);

	return miginfo;
}


void setMigInfo(const char *path, mig_info_t::state_t state)

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


void remMigInfo(const char *path)

{
	if ( removexattr(path, Const::OPEN_LTFS_EA_MIGINFO_INT.c_str()) == -1 )
		throw(errno);
}


mig_info_t getMigInfo(const char *path)

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


mig_info_t getMigInfoAt(int dirfd, const char *path)

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



bool needsRecovery(mig_info_t miginfo)

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

void recoverState(const char *path, mig_info_t::state_t state)

{
	// TODO
}

struct ltfsdm_dir_t {
	DIR *dir;
	struct dirent *dentry;
	off_t offset;
};


std::string souce_path(const char *path)

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
	if ( needsRecovery(miginfo) == true )
		 recoverState(fullpath.c_str(), miginfo.state);

	return fullpath;
}

int ltfsdm_getattr(const char *path, struct stat *statbuf)

{
	mig_info_t miginfo;

	memset(statbuf, 0, sizeof(struct stat));

	if ( lstat(souce_path(path).c_str(), statbuf) == -1 ) {
		return (-1*errno);
	}
	else {
		miginfo = getMigInfo(souce_path(path).c_str());
		if ( miginfo.state != mig_info_t::state_t::NO_STATE )
			statbuf->st_size = miginfo.statinfo.st_size;
		return 0;
	}
}

int ltfsdm_access(const char *path, int mask)

{
	if ( access(souce_path(path).c_str(), mask) == -1 )
		return (-1*errno);
	else
		return 0;
}

int ltfsdm_readlink(const char *path, char *buffer, size_t size)

{
	memset(buffer, 0, size);

	if ( readlink(souce_path(path).c_str(), buffer, size - 1) == -1 )
		return (-1*errno);
	else
		return 0;
}

int ltfsdm_opendir(const char *path, struct fuse_file_info *finfo)

{
	ltfsdm_dir_t *dirinfo = (ltfsdm_dir_t *) malloc(sizeof(ltfsdm_dir_t));

	if (dirinfo == NULL)
		return (-1*errno);

	if ( (dirinfo->dir = opendir(souce_path(path).c_str())) == 0 ) {
		free(dirinfo);
		return (-1*errno);
	}

	dirinfo->dentry = NULL;
	dirinfo->offset = 0;

	finfo->fh = (unsigned long) dirinfo;

	return 0;
}

int ltfsdm_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
						  off_t offset, struct fuse_file_info *finfo)

{
	struct stat statbuf;
	mig_info_t miginfo;
	off_t next;

	assert(path == NULL);

	ltfsdm_dir_t *dirinfo = (ltfsdm_dir_t *) finfo->fh;

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

int ltfsdm_releasedir(const char *path, struct fuse_file_info *finfo)

{
	assert(path == NULL);

	ltfsdm_dir_t *dirinfo = (ltfsdm_dir_t *) finfo->fh;

	closedir(dirinfo->dir);
	free(dirinfo);

	return 0;
}

int ltfsdm_mknod(const char *path, mode_t mode, dev_t rdev)

{
	struct fuse_context *fc;

	if ( mknod(souce_path(path).c_str(), mode, rdev) == -1 ) {
		return (-1*errno);
	}
	else {
		fc = fuse_get_context();
		if ( chown(souce_path(path).c_str(), fc->uid, fc->gid) == -1 )
			return (-1*errno);
	}
	return 0;
}

int ltfsdm_mkdir(const char *path, mode_t mode)

{
	struct fuse_context *fc;

	if ( mkdir(souce_path(path).c_str(), mode) == -1 ) {
		return (-1*errno);
	}
	else {
		fc = fuse_get_context();
		if ( chown(souce_path(path).c_str(), fc->uid, fc->gid) == -1 )
			return (-1*errno);
	}
	return 0;
}

int ltfsdm_unlink(const char *path)

{
	if ( unlink(souce_path(path).c_str()) == -1 )
		return (-1*errno);
	else
		return 0;
}

int ltfsdm_rmdir(const char *path)

{
	if ( rmdir(souce_path(path).c_str()) == -1 )
		return (-1*errno);
	else
		return 0;
}

int ltfsdm_symlink(const char *target, const char *linkpath)

{
	if ( symlink(target, souce_path(linkpath).c_str()) == -1 )
		return (-1*errno);
	else
		return 0;
}

int ltfsdm_rename(const char *oldpath, const char *newpath)

{
	if ( rename(souce_path(oldpath).c_str(), souce_path(newpath).c_str()) == -1 )
		return (-1*errno);
	else
		return 0;
}

int ltfsdm_link(const char *oldpath, const char *newpath)

{
	if ( link(souce_path(oldpath).c_str(), souce_path(newpath).c_str()) == -1 )
		return (-1*errno);
	else
		return 0;
}

int ltfsdm_chmod(const char *path, mode_t mode)

{
	if ( chmod(souce_path(path).c_str(), mode) == -1 )
		return (-1*errno);
	else
		return 0;
}

int ltfsdm_chown(const char *path, uid_t uid, gid_t gid)

{
	if ( lchown(souce_path(path).c_str(), uid, gid) == -1 )
		return (-1*errno);
	else
		return 0;
}

int ltfsdm_truncate(const char *path, off_t size)

{
	if ( truncate(souce_path(path).c_str(), size) == -1 )
		return (-1*errno);
	else
		return 0;
}

int ltfsdm_utimens(const char *path, const struct timespec times[2])

{
	if (  utimensat(0, souce_path(path).c_str(), times, AT_SYMLINK_NOFOLLOW) == -1 )
		return (-1*errno);
	else
		return 0;
}

int ltfsdm_open(const char *path, struct fuse_file_info *finfo)

{
	int fd = -1;

	if ( (fd = open(souce_path(path).c_str(), finfo->flags)) == -1 )
		return (-1*errno);

	finfo->fh = fd;

	return 0;
}

int ltfsdm_read(const char *path, char *buffer, size_t size, off_t offset,
					   struct fuse_file_info *finfo)

{
	ssize_t rsize = -1;
	mig_info_t migInfo;
	ssize_t attrsize;

	assert(path == NULL);

	if ( (attrsize = fgetxattr(finfo->fh, Const::OPEN_LTFS_EA_MIGINFO_INT.c_str(), (void *) &migInfo, sizeof(migInfo))) == -1 ) {
		if ( errno != ENODATA ) {
			return (-1*errno);
		}
	}

	if ( (rsize =pread(finfo->fh, buffer, size, offset)) == -1 )
		return (-1*errno);
	else
		return rsize;
}

int ltfsdm_read_buf(const char *path, struct fuse_bufvec **bufferp,
						   size_t size, off_t offset, struct fuse_file_info *finfo)

{
    struct fuse_bufvec *source;
	mig_info_t migInfo;
	ssize_t attrsize;

	assert(path == NULL);

	if ( (attrsize = fgetxattr(finfo->fh, Const::OPEN_LTFS_EA_MIGINFO_INT.c_str(), (void *) &migInfo, sizeof(migInfo))) == -1 ) {
		if ( errno != ENODATA ) {
			return (-1*errno);
		}
	}

	if ( migInfo.state == mig_info_t::state_t::MIGRATED ||
		 migInfo.state == mig_info_t::state_t::IN_RECALL ) {
		struct stat statbuf;
		unsigned int igen;

		if ( fstat(finfo->fh, &statbuf) == -1 ) {
			TRACE(Trace::error, errno);
			return (-1*errno);
		}

		if ( ioctl(finfo->fh, FS_IOC_GETVERSION, &igen) ) {
			TRACE(Trace::error, errno);
			throw(errno);
		}

		std::unique_lock<std::mutex> lock(trecall_mtx);

		recinfo_share.conn_info = new(struct conn_info_t);
		recinfo_share.toresident = false;
		recinfo_share.fsid = statbuf.st_dev;
		recinfo_share.igen = igen;
		recinfo_share.ino = statbuf.st_ino;
		recinfo_share.fd = dup(finfo->fh);

		std::stringstream procpath;

		procpath << "/proc/" << getpid() << "/fd/" << recinfo_share.fd;

		std::cout << "procpath: " << procpath.str() << std::endl;

		char sourcepath[PATH_MAX];

		memset(sourcepath, 0, sizeof(sourcepath));

		if ( readlink(procpath.str().c_str(), sourcepath, sizeof(sourcepath) - 1) == -1 ) {
			TRACE(Trace::error, errno);
			throw(errno);
		}

		std::cout << "sourcepath: " << sourcepath << std::endl;

		recinfo_share.filename = sourcepath;

		std::unique_lock<std::mutex> lock2(recinfo_share.conn_info->mtx);

		trecall_cond.notify_one();
		lock.unlock();

		std::cout << "waiting for the file being recalled" << std::endl;

		recinfo_share.conn_info->cond.wait(lock2);

		std::cout << "file is recalled: continue operation" << std::endl;
	}

    if ( (source = (fuse_bufvec*) malloc(sizeof(struct fuse_bufvec))) == NULL )
		return (-1*errno);

    *source = FUSE_BUFVEC_INIT(size);
    source->buf[0].flags = (fuse_buf_flags) (FUSE_BUF_IS_FD | FUSE_BUF_FD_SEEK);
    source->buf[0].fd = finfo->fh;
    source->buf[0].pos = offset;
    *bufferp = source;

    return 0;
}


int ltfsdm_write(const char *path, const char *buf, size_t size,
		     off_t offset, struct fuse_file_info *finfo)

{
	ssize_t wsize;
	mig_info_t migInfo;
	ssize_t attrsize;

	assert(path == NULL);

	if ( (attrsize = fgetxattr(finfo->fh, Const::OPEN_LTFS_EA_MIGINFO_INT.c_str(), (void *) &migInfo, sizeof(migInfo))) == -1 ) {
		if ( errno != ENODATA ) {
			return (-1*errno);
		}
	}

	if ( (wsize = pwrite(finfo->fh, buf, size, offset)) == -1 )
		return (-1*errno);
	else
		return wsize;
}

int ltfsdm_statfs(const char *path, struct statvfs *stbuf)

{
	if ( statvfs(souce_path(path).c_str(), stbuf) == -1 )
		return (-1*errno);
	else
		return 0;
}

int ltfsdm_release(const char *path, struct fuse_file_info *finfo)

{
	assert(path == NULL);

	if ( close(finfo->fh) == -1 )
		return (-1*errno);
	else
		return 0;
}

int ltfsdm_flush(const char *path, struct fuse_file_info *finfo)

{
	assert(path == NULL);

	if ( close(dup(finfo->fh)) == -1 )
		return (-1*errno);
	else
		return 0;
}

int ltfsdm_fsync(const char *path, int isdatasync,
						struct fuse_file_info *finfo)

{
	int rc;

	assert(path == NULL);

	if ( isdatasync )
	  rc = fdatasync(finfo->fh);
	else
	  rc = fsync(finfo->fh);

	if ( rc == -1 )
		return (-1*errno);
	else
		return 0;
}

int ltfsdm_fallocate(const char *path, int mode,
			off_t offset, off_t length, struct fuse_file_info *finfo)

{
	assert(path == NULL);

	if ( fallocate(finfo->fh, mode, offset, length) == -1 )
		return (-1*errno);
	else
		return 0;
}


int ltfsdm_setxattr(const char *path, const char *name, const char *value,
			size_t size, int flags)

{
	if ( std::string(name).find(Const::OPEN_LTFS_EA.c_str()) != std::string::npos )
		return (-1*ENOTSUP);

	if ( lsetxattr(souce_path(path).c_str(), name, value, size, flags) == -1 )
		return (-1*errno);
	else
		return 0;
}

int ltfsdm_getxattr(const char *path, const char *name, char *value,
			size_t size)

{
	ssize_t attrsize;
	//struct fuse_context *fc = fuse_get_context();

	// additionally to compare fc->pid with getpid() does not work
	// since fc->pid returns a thread id

	if ( Const::OPEN_LTFS_EA_FSINFO.compare(name) == 0 ) {
		strncpy(value, souce_path(path).c_str(), PATH_MAX);
		size = strlen(value) + 1;
		return size;
	}
	else if ( (attrsize = lgetxattr(souce_path(path).c_str(), name, value, size)) == -1 ) {
		return (-1*errno);
	}
	else if ( std::string(name).find(Const::OPEN_LTFS_EA.c_str()) != std::string::npos ) {
		return (-1*ENOTSUP);
	}
	else {
		return attrsize;
	}
}

int ltfsdm_listxattr(const char *path, char *list, size_t size)
{
	ssize_t attrsize;

	if ( (attrsize = llistxattr(souce_path(path).c_str(), list, size)) == -1 )
		return (-1*errno);
	else
		return attrsize;
}

int ltfsdm_removexattr(const char *path, const char *name)
{
	if ( std::string(name).find(Const::OPEN_LTFS_EA.c_str()) != std::string::npos )
		return (-1*ENOTSUP);

	if ( lremovexattr(souce_path(path).c_str(), name) == -1 )
		return (-1*errno);
	else
		return 0;
}

void *ltfsdm_init(struct fuse_conn_info *conn)

{
	struct fuse_context *fc = fuse_get_context();
	return fc->private_data;
}

struct fuse_operations FuseFS::init_operations()

{
	struct fuse_operations ltfsdm_operations;

	memset(&ltfsdm_operations, 0, sizeof(ltfsdm_operations));

    ltfsdm_operations.init       	= ltfsdm_init;

	ltfsdm_operations.getattr		= ltfsdm_getattr;
	ltfsdm_operations.access		= ltfsdm_access;
	ltfsdm_operations.readlink		= ltfsdm_readlink;

	ltfsdm_operations.opendir		= ltfsdm_opendir;
	ltfsdm_operations.readdir		= ltfsdm_readdir;
	ltfsdm_operations.releasedir	= ltfsdm_releasedir;

	ltfsdm_operations.mknod			= ltfsdm_mknod;
	ltfsdm_operations.mkdir			= ltfsdm_mkdir;
	ltfsdm_operations.symlink		= ltfsdm_symlink;
	ltfsdm_operations.unlink		= ltfsdm_unlink;
	ltfsdm_operations.rmdir			= ltfsdm_rmdir;
	ltfsdm_operations.rename		= ltfsdm_rename;
	ltfsdm_operations.link			= ltfsdm_link;
	ltfsdm_operations.chmod			= ltfsdm_chmod;
	ltfsdm_operations.chown			= ltfsdm_chown;
	ltfsdm_operations.truncate		= ltfsdm_truncate;
	ltfsdm_operations.utimens		= ltfsdm_utimens;
	ltfsdm_operations.open			= ltfsdm_open;
	ltfsdm_operations.read			= ltfsdm_read;
	ltfsdm_operations.read_buf      = ltfsdm_read_buf;
	ltfsdm_operations.write			= ltfsdm_write;
	ltfsdm_operations.statfs		= ltfsdm_statfs;
	ltfsdm_operations.release		= ltfsdm_release;
	ltfsdm_operations.flush         = ltfsdm_flush;
	ltfsdm_operations.fsync			= ltfsdm_fsync;
	ltfsdm_operations.fallocate		= ltfsdm_fallocate;
	ltfsdm_operations.setxattr		= ltfsdm_setxattr;
	ltfsdm_operations.getxattr		= ltfsdm_getxattr;
	ltfsdm_operations.listxattr		= ltfsdm_listxattr;
	ltfsdm_operations.removexattr	= ltfsdm_removexattr;

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
