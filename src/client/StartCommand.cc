#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <limits.h>
#include <libgen.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>

#ifdef __APPLE__
#include <mach-o/dyld.h>
#endif

#include <string>
#include <sstream>
#include "src/common/messages/Message.h"
#include "src/common/tracing/Trace.h"
#include "src/common/errors/errors.h"
#include "src/common/const/Const.h"

#include "src/common/comm/ltfsdm.pb.h"
#include "src/common/comm/LTFSDmComm.h"

#include "OpenLTFSCommand.h"
#include "StartCommand.h"

void StartCommand::printUsage()

{
	INFO(LTFSDMC0006I);
}

void StartCommand::determineServerPath()

{
	char exepath[PATH_MAX];

	TRACE(Trace::little, Const::SERVER_COMMAND);

#ifdef __linux__
	char *exelnk = (char*) "/proc/self/exe";

	if ( readlink(exelnk, exepath, PATH_MAX) == -1 ) {
		MSG(LTFSDMC0021E);
		throw Error::LTFSDM_GENERAL_ERROR;
	}
#elif __APPLE__
	uint32_t size = PATH_MAX;
	if ( _NSGetExecutablePath(exepath, &size) != 0 ) {
		MSG(LTFSDMC0021E);
		TRACE(Trace::error, errno);
		throw Error::LTFSDM_GENERAL_ERROR;
	}
#else
#error "unsupported platform"
#endif

	serverPath << dirname(exepath) << "/" << Const::SERVER_COMMAND;

	TRACE(Trace::little, serverPath.str());
}

void StartCommand::startServer()

{
	struct stat statbuf;
	FILE *ltfsdmd = NULL;
	char line[Const::OUTPUT_LINE_SIZE];
	int ret;

	if ( stat(serverPath.str().c_str(), &statbuf ) == -1 ) {
		MSG(LTFSDMC0021E);
		TRACE(Trace::error, serverPath.str());
		TRACE(Trace::error, errno);
		throw Error::LTFSDM_GENERAL_ERROR;
	}

	ltfsdmd = popen(serverPath.str().c_str(), "r");

	if( !ltfsdmd ) {
		MSG(LTFSDMC0022E);
		TRACE(Trace::error, errno);
		throw Error::LTFSDM_GENERAL_ERROR;
	}

    while( fgets(line, sizeof(line), ltfsdmd) ) {
		INFO(LTFSDMC0024I, line);
    }


	ret = pclose(ltfsdmd);

    if(  !WIFEXITED(ret) || WEXITSTATUS(ret) ) {
		MSG(LTFSDMC0023E, WEXITSTATUS(ret));
		TRACE(Trace::error, ret);
		TRACE(Trace::error, WIFEXITED(ret));
		TRACE(Trace::error, WEXITSTATUS(ret));
		throw Error::LTFSDM_GENERAL_ERROR;
	}
}

void StartCommand::doCommand(int argc, char **argv)

{
	if ( argc > 1 ) {
		printUsage();
		throw Error::LTFSDM_GENERAL_ERROR;
	}

	determineServerPath();
	startServer();
}
