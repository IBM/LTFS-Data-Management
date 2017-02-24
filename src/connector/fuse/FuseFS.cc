#include <stdio.h> // for rename
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <dirent.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/xattr.h>
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

#include "FuseFS.h"

thread_local std::string lsourcedir = "";

struct ltfsdm_dir_t {
	DIR *dir;
	struct dirent *dentry;
	off_t offset;
};

bool FuseFS::isMigrated(int fd)

{
	int val;

	if ( fgetxattr(fd, "trusted.openltfs.migrated", &val, sizeof(val)) == -1 ) {
		if ( errno == ENODATA )
			return false;
//std::cerr << "unable to detect the migration state, errno: " << errno << std::endl;
		return true;
	}

	return (val == 1);
}

std::string souce_path(const char *path)

{
	char thread_name[16];
	char sourcedir[PATH_MAX];

	if ( lsourcedir.compare("") == 0 ) {
		memset(thread_name, 0, sizeof(thread_name));
		pthread_getname_np(pthread_self(), (char *) thread_name, sizeof(thread_name));
		std::stringstream fdpath;
		fdpath << "/proc/" << getpid() << "/fd/" << thread_name+3;
		memset(sourcedir, 0, sizeof(sourcedir));
		if ( readlink(fdpath.str().c_str(), sourcedir, sizeof(sourcedir)-1) == -1 )
			return "";
		lsourcedir = sourcedir;
	}

	return lsourcedir + std::string(path);
}

int ltfsdm_getattr(const char *path, struct stat *statbuf)

{
	memset(statbuf, 0, sizeof(struct stat));

	if ( lstat(souce_path(path).c_str(), statbuf) == -1 )
		return (-1*errno);
	else
		return 0;
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

	assert(path == NULL);

	if ( (rsize =pread(finfo->fh, buffer, size, offset)) == -1 )
		return (-1*errno);
	else
		return rsize;
}

int ltfsdm_read_buf(const char *path, struct fuse_bufvec **bufferp,
						   size_t size, off_t offset, struct fuse_file_info *finfo)

{
    struct fuse_bufvec *source;

	assert(path == NULL);

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
	if ( lsetxattr(souce_path(path).c_str(), name, value, size, flags) == -1 )
		return (-1*errno);
	else
		return 0;
}

int ltfsdm_getxattr(const char *path, const char *name, char *value,
			size_t size)

{
	ssize_t attrsize;

	if ( (attrsize = lgetxattr(souce_path(path).c_str(), name, value, size)) == -1 )
		return (-1*errno);
	else
		return attrsize;
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
	if ( lremovexattr(souce_path(path).c_str(), name) == -1 )
		return (-1*errno);
	else
		return 0;
}

void *ltfsdm_init(struct fuse_conn_info *conn)

{
	return NULL;
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

void FuseFS::run()

{
	std::stringstream threadName;

	threadName << "FS:"  << fd;
	pthread_setname_np(pthread_self(), threadName.str().c_str());

	fuse_loop_mt(openltfs);
}

FuseFS::FuseFS(std::string sourcedir_, std::string mountpt_) : sourcedir(sourcedir_), mountpt(mountpt_)

{
	std::stringstream options;
	struct fuse_args fargs = FUSE_ARGS_INIT(0, NULL);
	struct fuse_operations ltfsdm_operations = init_operations();

	MSG(LTFSDMF0001I, sourcedir, mountpt);

	if ( (fd = open(sourcedir.c_str(), O_RDONLY)) == -1 ) {
		TRACE(Trace::error, errno);
		MSG(LTFSDMF0010E, sourcedir);
		throw(Error::LTFSDM_FS_ADD_ERROR);
	}

	fuse_opt_add_arg(&fargs, mountpt.c_str());

	options << "-ouse_ino,fsname=OpenLTFS:" << sourcedir << ",nopath,default_permissions,allow_other";

	fuse_opt_add_arg(&fargs, options.str().c_str());
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

	openltfs = fuse_new(openltfsch, &fargs,  &ltfsdm_operations, sizeof(ltfsdm_operations), NULL);

	if ( openltfs == NULL ) {
		MSG(LTFSDMF0006E);
		throw(Error::LTFSDM_FS_ADD_ERROR);
	}

	fusefs = new std::thread(&FuseFS::run, this);
}


FuseFS::~FuseFS()

{
	MSG(LTFSDMF0007I);
	fuse_exit(openltfs);
	fuse_unmount(mountpt.c_str(), openltfsch);
	fusefs->join();
	close(fd);
}
