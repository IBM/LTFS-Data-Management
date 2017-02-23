#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
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

FuseFS *F1 = NULL;
FuseFS *F2 = NULL;

Connector::Connector(bool cleanup)

{
	if ( cleanup ) {
		F1 = new FuseFS("/mnt/lxfs/orig1", "/mnt/lxfs/fuse1");
		F2 = new FuseFS("/mnt/lxfs/orig2", "/mnt/lxfs/fuse2");
	}
}

Connector::~Connector()

{
	if ( F1 )
		delete(F1);
	if ( F2 )
		delete(F2);
}

void Connector::initTransRecalls()

{
}

Connector::rec_info_t Connector::getEvents()

{
	Connector::rec_info_t rinfo;
	return rinfo;
}

void Connector::respondRecallEvent(rec_info_t recinfo, bool success)

{
}

void Connector::terminate()

{
}

FsObj::FsObj(std::string fileName)
	: handle(NULL), handleLength(0), isLocked(false), handleFree(true)

{
}

FsObj::FsObj(unsigned long long fsId, unsigned int iGen, unsigned long long iNode)
	: handle(NULL), handleLength(0), isLocked(false), handleFree(true)

{
}

FsObj::~FsObj()

{
}

bool FsObj::isFsManaged()

{
	bool managed = false;
	return managed;
}

void FsObj::manageFs(bool setDispo)

{
}

struct stat FsObj::stat()

{
	struct stat statbuf;

	return statbuf;
}

unsigned long long FsObj::getFsId()

{
	unsigned long long fsid = 0;

	return fsid;
}

unsigned int FsObj::getIGen()

{
	unsigned int igen = 0;

	return igen;
}

unsigned long long FsObj::getINode()

{
	unsigned long long ino = 0;

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

	return rsize;
}

long FsObj::write(long offset, unsigned long size, char *buffer)

{
	long wsize = 0;

	return wsize;
}


void FsObj::addAttribute(mig_attr_t value)

{
}

void FsObj::remAttribute()

{
}

FsObj::mig_attr_t FsObj::getAttribute()

{
	FsObj::mig_attr_t attr;
	memset(&attr, 0, sizeof(mig_attr_t));
	return attr;
}

void FsObj::preparePremigration()

{
}

void FsObj::finishRecall(FsObj::file_state fstate)

{
}


void FsObj::prepareStubbing()

{
}


void FsObj::stub()

{
}


FsObj::file_state FsObj::getMigState()
{
	FsObj::file_state state = FsObj::RESIDENT;

	return state;
}
