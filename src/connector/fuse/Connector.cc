#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mount.h>
#include <linux/fs.h>
#include <unistd.h>
#include <sys/xattr.h>
#include <string.h>
#include <limits.h>
#include <errno.h>

#include <fuse.h>

#include <string>
#include <sstream>
#include <atomic>
#include <typeinfo>
#include <map>
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

typedef struct {
	unsigned long long fsid;
	unsigned int igen;
	unsigned long long ino;
} fuid_t;

struct ltstr
{
	bool operator()(const fuid_t fuid1, const fuid_t fuid2) const
	{
        return ( fuid1.ino < fuid2.ino )
			|| (( fuid1.ino == fuid2.ino )
				&& (( fuid1.igen < fuid2.igen )
					|| (( fuid1.igen == fuid2.igen )
						&& (( fuid1.fsid < fuid2.fsid )))));
	}
};

std::map<fuid_t, int, ltstr> fuidMap;

std::mutex mtx;

std::vector<FuseFS*> managedFss;

Connector::Connector(bool cleanup)

{
	clock_gettime(CLOCK_REALTIME, &starttime);
}

Connector::~Connector()

{
	std::string mountpt;

	for(auto const& fn: managedFss) {
		mountpt = fn->getMountPoint();
		delete(fn);
		if ( rmdir(mountpt.c_str()) == -1 )
			MSG(LTFSDMF0008W, mountpt.c_str());
	}
}

void Connector::initTransRecalls()

{
}

Connector::rec_info_t Connector::getEvents()

{
	Connector::rec_info_t rinfo;
	sleep(1);
	return rinfo;
}

void Connector::respondRecallEvent(rec_info_t recinfo, bool success)

{
}

void Connector::terminate()

{
}

struct FuseHandle {
	std::string fileName;
	std::string sourcePath;
	int fd;
};

FsObj::FsObj(std::string fileName)
	: handle(NULL), handleLength(0), isLocked(false), handleFree(true)

{
	FuseHandle *fh = new FuseHandle();
	char source_path[PATH_MAX];

	fh->fileName = fileName;

	memset(source_path, 0, sizeof(source_path));
	if ( getxattr(fileName.c_str(), Const::OPEN_LTFS_EA_FSINFO.c_str(), source_path, sizeof(source_path)-1) == -1 ) {
		if ( errno != ENODATA )
			TRACE(Trace::error, errno);
		fh->fd = -1;
	}
	else {
		fh->fd = open(source_path, O_RDWR);
		if ( fh->fd == -1 )
			TRACE(Trace::error, errno);
	}

	fh->sourcePath = source_path;

	handle = (void *) fh;
	handleLength = fileName.size();
}

FsObj::FsObj(unsigned long long fsId, unsigned int iGen, unsigned long long iNode)
	: handle(NULL), handleLength(0), isLocked(false), handleFree(true)

{
}

FsObj::~FsObj()

{
	FuseHandle *fh = (FuseHandle *) handle;

	if ( fh->fd != -1 )
		close(fh->fd);

	delete(fh);
}

bool FsObj::isFsManaged()

{
	int val;
	FuseHandle *fh = (FuseHandle *) handle;
	std::unique_lock<std::mutex> lock(mtx);

	if ( getxattr(fh->fileName.c_str(), Const::OPEN_LTFS_EA_MANAGED.c_str(), &val, sizeof(val)) == -1 ) {
		if ( errno == ENODATA )
			return false;
		return true;
	}

	return (val == 1);
}

void FsObj::manageFs(bool setDispo, struct timespec starttime)

{
	int managed = 1;
	FuseHandle *fh = (FuseHandle *) handle;
	std::string mountedPoint = fh->fileName + std::string(".managed");
	struct stat statbuf;
	FuseFS *FS;

	umount2(mountedPoint.c_str(), MNT_FORCE);

	if ( ::stat(mountedPoint.c_str(), &statbuf) == 0 ) {
		if ( rmdir(mountedPoint.c_str()) == -1 ) {
			TRACE(Trace::error, errno);
			managed = 0;
		}
	}

	if ( managed == 1 ) {
		if ( mkdir(mountedPoint.c_str(), 700) == -1 ) {
			TRACE(Trace::error, errno);
			managed = 0;
		}
	}

	if ( managed == 1 ) {
		try {
			FS = new FuseFS(fh->fileName, mountedPoint, starttime);
		}
		catch(int error) {
			managed = 0;
		}
	}

	std::unique_lock<std::mutex> lock(mtx);

	if ( setxattr(fh->fileName.c_str(), Const::OPEN_LTFS_EA_MANAGED.c_str(), &managed, sizeof(managed), 0) == -1 ) {
		TRACE(Trace::error, errno);
		MSG(LTFSDMF0009E, fh->fileName.c_str());
		if ( managed == 1 )
			delete(FS);
		throw(Error::LTFSDM_FS_ADD_ERROR);
	}

	if ( managed == 0 ) {
		rmdir(mountedPoint.c_str());
		MSG(LTFSDMF0009E, fh->fileName.c_str());
		throw(Error::LTFSDM_FS_ADD_ERROR);
	}
	else {
		managedFss.push_back(FS);
	}
}

struct stat FsObj::stat()

{
	struct stat statbuf;
	mig_info_t miginfo;

	FuseHandle *fh = (FuseHandle *) handle;

	miginfo = getMigInfo(fh->sourcePath.c_str());

	if ( miginfo.state !=mig_info_t::state_t::NO_STATE ) {
		statbuf = miginfo.statinfo;
	}
	else {
		memset(&statbuf, 0, sizeof(statbuf));
		if ( fstat(fh->fd, &statbuf) == -1 ) {
			TRACE(Trace::error, errno);
			throw(errno);
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
	if ( fstat(fh->fd, &statbuf) == -1 ) {
		TRACE(Trace::error, errno);
		throw(errno);
	}

	// TODO
	fsid = statbuf.st_dev;

	return fsid;
}

unsigned int FsObj::getIGen()

{
	unsigned int igen = 0;
	FuseHandle *fh = (FuseHandle *) handle;

	if ( ioctl(fh->fd, FS_IOC_GETVERSION, &igen) ) {
		TRACE(Trace::error, errno);
		throw(errno);
	}

	return igen;
}

unsigned long long FsObj::getINode()

{
	unsigned long long ino = 0;
	struct stat statbuf;
	FuseHandle *fh = (FuseHandle *) handle;

	memset(&statbuf, 0, sizeof(statbuf));
	if ( fstat(fh->fd, &statbuf) == -1 ) {
		TRACE(Trace::error, errno);
		throw(errno);
	}

	ino = statbuf.st_ino;

	return ino;
}

std::string FsObj::getTapeId()

{
	std::stringstream sstr;

	sstr << "DV148" << random() % 2 <<  "L6";

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

	if ( rsize == -1 ) {
		TRACE(Trace::error, errno);
		throw(errno);
	}

	return rsize;
}

long FsObj::write(long offset, unsigned long size, char *buffer)

{
	long wsize = 0;
	FuseHandle *fh = (FuseHandle *) handle;
	wsize = ::write(fh->fd, buffer, size);

	if ( wsize == -1 ) {
		TRACE(Trace::error, errno);
		throw(errno);
	}

	return wsize;
}


void FsObj::addAttribute(mig_attr_t value)

{
	FuseHandle *fh = (FuseHandle *) handle;

	if ( fsetxattr(fh->fd, Const::OPEN_LTFS_EA_MIGINFO_EXT.c_str(), (void *) &value, sizeof(value), 0) == -1 ) {
		TRACE(Trace::error, errno);
		throw(errno);
	}
}

void FsObj::remAttribute()

{
	FuseHandle *fh = (FuseHandle *) handle;

	setMigInfo(fh->sourcePath.c_str(), mig_info_t::state_t::NO_STATE);

	if ( fremovexattr(fh->fd, Const::OPEN_LTFS_EA_MIGINFO_EXT.c_str()) == -1 ) {
		TRACE(Trace::error, errno);
		throw(errno);
	}
}

FsObj::mig_attr_t FsObj::getAttribute()

{
	FuseHandle *fh = (FuseHandle *) handle;
	FsObj::mig_attr_t value;
	memset(&value, 0, sizeof(mig_attr_t));

	if ( fgetxattr(fh->fd, Const::OPEN_LTFS_EA_MIGINFO_EXT.c_str(), (void *) &value, sizeof(value)) == -1 ) {
		if ( errno != ENODATA ) {
			TRACE(Trace::error, errno);
			throw(errno);
		}
	}

	return value;
}

void FsObj::preparePremigration()

{
	FuseHandle *fh = (FuseHandle *) handle;

	setMigInfo(fh->sourcePath.c_str(), mig_info_t::state_t::IN_MIGRATION);
}

void FsObj::finishRecall(FsObj::file_state fstate)

{
	FuseHandle *fh = (FuseHandle *) handle;

	if ( fstate == FsObj::PREMIGRATED ) {
		setMigInfo(fh->sourcePath.c_str(), mig_info_t::state_t::PREMIGRATED);
	}
	else {
		remMigInfo(fh->sourcePath.c_str());
	}
}


void FsObj::prepareStubbing()

{
	FuseHandle *fh = (FuseHandle *) handle;

	setMigInfo(fh->sourcePath.c_str(), mig_info_t::state_t::STUBBING);
}


void FsObj::stub()

{
	FuseHandle *fh = (FuseHandle *) handle;
	struct stat statbuf;

	if ( fstat(fh->fd, &statbuf) == -1 ) {
		TRACE(Trace::error, errno);
		throw(errno);
	}

	if ( ftruncate(fh->fd, 0) == -1 ) {
		TRACE(Trace::error, errno);
		throw(errno);
	}


	// if ( ftruncate(fh->fd, statbuf.st_size) == -1 ) {
	// 	TRACE(Trace::error, errno);
	// 	throw(errno);
	// }

	setMigInfo(fh->sourcePath.c_str(), mig_info_t::state_t::MIGRATED);
}


FsObj::file_state FsObj::getMigState()
{
	FuseHandle *fh = (FuseHandle *) handle;
	FsObj::file_state state = FsObj::RESIDENT;
	mig_info_t miginfo;

	miginfo = getMigInfo(fh->sourcePath.c_str());

	switch (miginfo.state) {
		case mig_info_t::state_t::NO_STATE:
		case mig_info_t::state_t::IN_MIGRATION:
			state = FsObj::RESIDENT;
			break;
		case mig_info_t::state_t::MIGRATED:
		case mig_info_t::state_t::IN_RECALL:
			state = FsObj::MIGRATED;
			break;
		case mig_info_t::state_t::PREMIGRATED:
		case mig_info_t::state_t::STUBBING:
			state = FsObj::PREMIGRATED;
	}

	return state;
}
