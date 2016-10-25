#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include <string>

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
