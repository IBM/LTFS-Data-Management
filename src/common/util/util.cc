#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <mntent.h>
#include <sys/resource.h>

#include <string>
#include <fstream>
#include <set>
#include <vector>

#include "src/common/messages/Message.h"
#include "src/common/tracing/Trace.h"
#include "src/common/errors/errors.h"
#include "src/common/const/Const.h"

#include "util.h"


void mkTmpDir()

{
	struct stat statbuf;

	if ( stat(Const::LTFSDM_TMP_DIR.c_str(), &statbuf) != 0 ) {
		if ( mkdir(Const::LTFSDM_TMP_DIR.c_str(), 0700) != 0 ) {
			std::cerr << messages[LTFSDMX0006E] << Const::LTFSDM_TMP_DIR << std::endl;
			throw(Error::LTFSDM_GENERAL_ERROR);
		}
	}
	else if ( !S_ISDIR(statbuf.st_mode) ) {
		std::cerr << Const::LTFSDM_TMP_DIR << messages[LTFSDMX0007E] << std::endl;
		throw(Error::LTFSDM_GENERAL_ERROR);
	}
}


void LTFSDM::init()

{
	mkTmpDir();
	messageObject.init();
	traceObject.init();
}

std::vector<std::string> LTFSDM::getTapeIds()

{
	std::vector<std::string> tapeIds;
	std::ifstream tpfile("/tmp/tapeids");
	std::string line;

	while (std::getline(tpfile, line))
	{
		tapeIds.push_back(line);
	}

	return tapeIds;
}

std::set<std::string> LTFSDM::getFs()

{
	unsigned int    buflen = 32 * 1024;
	char           *buffer = NULL;
	struct mntent   mntbuf;
	FILE           *MNTINFO;
	std::set<std::string> mountList;

	MNTINFO = setmntent("/proc/mounts", "r");
	if( !MNTINFO ) {
		throw int(errno);
	}

	buffer = (char *) malloc(buflen);

	if ( buffer == NULL )
		throw int(Error::LTFSDM_GENERAL_ERROR);

	while (getmntent_r(MNTINFO, &mntbuf, buffer, buflen))
		if (!strcmp(mntbuf.mnt_type, "xfs") ||
			!strcmp(mntbuf.mnt_type, "ext4"))
			mountList.insert(std::string(mntbuf.mnt_dir));

	endmntent(MNTINFO);

	free(buffer);

	return mountList;
}
