#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>

#include <string>
#include <sstream>

#include "src/common/const/Const.h"
#include "src/connector/Connector.h"

Connector::Connector(bool cleanup)

{
}

Connector::~Connector()

{
}

void Connector::initTransRecalls()

{
}

Connector::rec_info_t Connector::getEvents()

{
	rec_info_t recinfo;

	return recinfo;
}

void Connector::terminate()

{
}

FsObj::FsObj(std::string fileName)

{
	handle = nullptr;
	handleLength = 0;
}

FsObj::FsObj(unsigned long long fsId, unsigned int iGen, unsigned long long iNode)

{
	handle = nullptr;
	handleLength = 0;
}

FsObj::~FsObj()

{
}

struct stat FsObj::stat()

{
	struct stat statbuf;

	memset(&statbuf, 0, sizeof(statbuf));

	statbuf.st_mode = S_IFREG;

	return statbuf;
}

unsigned long long FsObj::getFsId()

{
	return 1;
}

unsigned int FsObj::getIGen()

{
	return 2;
}

unsigned long long FsObj::getINode()

{
	return 3;
}

std::string FsObj::getTapeId()

{
	std::stringstream sstr;

	sstr << "TAPE0" << random() % 4;

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
	return 0;
}

long FsObj::write(long offset, unsigned long size, char *buffer)

{
	return 0;
}

void FsObj::addAttribute(attr_t value)

{
}

void FsObj::remAttribute()

{
}

FsObj::attr_t  FsObj::getAttribute()

{
	FsObj::attr_t attr;

	memset(&attr, 0, sizeof(attr));
	return attr;
}

void FsObj::preparePremigration()

{
}

void FsObj::finishRecall(file_state fstate)

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
	return FsObj::RESIDENT;
}
