#include <unistd.h>
#include <string>

#include "src/common/messages/Message.h"
#include "src/common/tracing/Trace.h"
#include "src/common/errors/errors.h"

#include "OpenLTFSCommand.h"
#include "InfoRequestsCommand.h"

void InfoRequestsCommand::printUsage()
{
	INFO(LTFSDMC0009I);
}

void InfoRequestsCommand::doCommand(int argc, char **argv)
{
	processOptions(argc, argv);

	TRACE(Trace::little, *argv);
	TRACE(Trace::little, argc);
	TRACE(Trace::little, optind);

	if( argc != optind ) {
		printUsage();
		throw(LTFSDMErr::LTFSDM_GENERAL_ERROR);
	}
	else if (requestNumber < 0 && waitForCompletion) {
		printUsage();
		throw(LTFSDMErr::LTFSDM_GENERAL_ERROR);
	}

	return;
}
