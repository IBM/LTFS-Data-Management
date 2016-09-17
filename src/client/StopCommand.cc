#include <string>
#include "src/common/messages/messages.h"
#include "src/common/errors/errors.h"

#include "OpenLTFSCommand.h"
#include "StopCommand.h"

void StopCommand::printUsage()
{
	MSG_INFO(OLTFSC0007I);
}

void StopCommand::doCommand(int argc, char **argv)
{
	if ( argc > 1 ) {
		printUsage();
		throw OLTFSErr::OLTFS_GENERAL_ERROR;
	}
}
