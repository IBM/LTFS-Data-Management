#include <unistd.h>
#include <string>

#include "src/common/messages/messages.h"
#include "src/common/tracing/Trace.h"
#include "src/common/errors/errors.h"

#include "OpenLTFSCommand.h"
#include "InfoRequestsCommand.h"

void InfoRequestsCommand::printUsage()
{
	MSG_INFO(OLTFSC0009I);
}

void InfoRequestsCommand::doCommand(int argc, char **argv)
{
	processOptions(argc, argv);

	TRACE(0, *argv);
	TRACE(0, argc);
	TRACE(0, optind);

	if( argc != optind ) {
		printUsage();
		throw(OLTFSErr::OLTFS_GENERAL_ERROR);
	}
	else if (requestNumber < 0 && waitForCompletion) {
		printUsage();
		throw(OLTFSErr::OLTFS_GENERAL_ERROR);
	}

	return;
}
