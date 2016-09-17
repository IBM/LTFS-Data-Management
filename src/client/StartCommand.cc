#include <string>
#include "src/common/messages/messages.h"
#include "src/common/errors/errors.h"

#include "OpenLTFSCommand.h"
#include "StartCommand.h"

void StartCommand::printUsage()
{
	MSG_INFO(OLTFSC0006I);
}

void StartCommand::doCommand(int argc, char **argv)
{
	if ( argc > 1 ) {
		printUsage();
		throw OLTFSErr::OLTFS_GENERAL_ERROR;
	}
}
