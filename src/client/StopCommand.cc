#include <string>
#include "src/common/messages/Message.h"
#include "src/common/errors/errors.h"

#include "src/common/comm/ltfsdm.pb.h"
#include "src/common/comm/LTFSDmComm.h"

#include "OpenLTFSCommand.h"
#include "StopCommand.h"

void StopCommand::printUsage()
{
	INFO(LTFSDMC0007I);
}

void StopCommand::doCommand(int argc, char **argv)
{
	if ( argc > 1 ) {
		printUsage();
		throw LTFSDMErr::LTFSDM_GENERAL_ERROR;
	}
}
