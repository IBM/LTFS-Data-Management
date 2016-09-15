#include <unistd.h>
#include <string>

#include "src/common/messages/messages.h"
#include "src/common/tracing/trace.h"
#include "src/common/errors/errors.h"

#include "Operation.h"
#include "Migration.h"

Migration::Migration() :
	waitForCompletion(false), preMigrate(false), requestNumber(-1), fileList(""), directoryName("")

{
}

void Migration::printUsage()
{
	MSG_INFO(OLTFSC0001I);
}

void Migration::doOperation(int argc, char **argv)
{
	int opt;

	opterr = 0;

	if ( argc == 1 ) {
		MSG_INFO(OLTFSC0018E);
		goto error;
	}

	while (( opt = getopt(argc, argv, ":hwpr:f:d:")) != -1 ) {
		switch( opt ) {
			case 'h':
				printUsage();
				return;
			case 'w':
				waitForCompletion = true;
				break;
			case 'p':
				preMigrate = true;
				break;
			case 'r':
				requestNumber = strtoul(optarg, NULL, 0);;
				break;
			case 'f':
				fileList = std::string(optarg);
				break;
			case 'd':
				directoryName = std::string(optarg);
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

	if ( fileList.compare("") && directoryName.compare("") ) {
		MSG_INFO(OLTFSC0015E);
		goto error;
	}

	if (optind != argc) {
		if (fileList.compare("")) {
			MSG_INFO(OLTFSC0016E);
			goto error;
		}
		if (directoryName.compare("")) {
			MSG_INFO(OLTFSC0017E);
			goto error;
		}
	}

	TRACE(0, waitForCompletion);
	TRACE(0, preMigrate);
	TRACE(0, requestNumber);
	TRACE(0, fileList);
	TRACE(0, directoryName);

	return;

error:
	printUsage();
	throw(OLTFSErr::OLTFS_GENERAL_ERROR);
}
