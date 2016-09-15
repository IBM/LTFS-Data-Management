#include <string>
#include "src/common/messages/messages.h"
#include "src/common/errors/errors.h"

#include "Operation.h"
#include "Stop.h"

Stop::Stop()

{
}

void Stop::printUsage()
{
	MSG_INFO(OLTFSC0007I);
}

void Stop::doOperation(int argc, char **argv)
{
	if ( argc > 1 ) {
		printUsage();
		throw OLTFSErr::OLTFS_GENERAL_ERROR;
	}
}
