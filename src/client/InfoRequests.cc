#include <unistd.h>
#include <string>

#include "src/common/messages/messages.h"
#include "src/common/tracing/trace.h"
#include "src/common/errors/errors.h"

#include "Operation.h"
#include "InfoRequests.h"

void InfoRequests::printUsage()
{
	MSG_INFO(OLTFSC0009I);
}

void InfoRequests::doOperation(int argc, char **argv)
{
	int opt;

	opterr = 0;

	if ( argc == 1 ) {
		MSG_INFO(OLTFSC0018E);
		goto error;
	}

	while (( opt = getopt(argc, argv, ":hwr")) != -1 ) {
		switch( opt ) {
			case 'h':
				printUsage();
				return;
			case 'w':
				waitForCompletion = true;
				break;
			case 'r':
				requestNumber = strtoul(optarg, NULL, 0);;
				break;
			case ':':
				MSG_INFO(OLTFSC0014E);
				printUsage();
				throw(OLTFSErr::OLTFS_GENERAL_ERROR);
			default:
				MSG_INFO(OLTFSC0013E);
				goto error;
		}
	}


	TRACE(0, waitForCompletion);
	TRACE(0, requestNumber);

	return;

error:
	printUsage();
	throw(OLTFSErr::OLTFS_GENERAL_ERROR);
}
