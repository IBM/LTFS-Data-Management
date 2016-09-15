#include <string>
#include "src/common/messages/messages.h"
#include "src/common/errors/errors.h"

#include "Operation.h"
#include "Start.h"

Start::Start()

{
}

void Start::printUsage()
{
	MSG_INFO(OLTFSC0006I);
}

void Start::doOperation(int argc, char **argv)
{
	if ( argc > 1 ) {
		printUsage();
		throw OLTFSErr::OLTFS_GENERAL_ERROR;
	}
}
