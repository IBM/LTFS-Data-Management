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
	if ( argc == 1 ) {
		MSG_INFO(OLTFSC0018E);
		goto error;
	}

	processOptions(argc, argv);

	TRACE(0, waitForCompletion);
	TRACE(0, requestNumber);

	return;

error:
	printUsage();
	throw(OLTFSErr::OLTFS_GENERAL_ERROR);
}
