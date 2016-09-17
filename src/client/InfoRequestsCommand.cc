#include <unistd.h>
#include <string>

#include "src/common/messages/messages.h"
#include "src/common/tracing/trace.h"
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

	TRACE(0, waitForCompletion);
	TRACE(0, requestNumber);

	return;
}
