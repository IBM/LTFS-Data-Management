#include <unistd.h>
#include <string>

#include "src/common/messages/messages.h"
#include "src/common/tracing/trace.h"
#include "src/common/errors/errors.h"

#include "Operation.h"
#include "Recall.h"

void Recall::printUsage()
{
	MSG_INFO(OLTFSC0002I);
}

void Recall::doOperation(int argc, char **argv)
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
	else if ( !fileList.compare("") && !directoryName.compare("") ) {
		// a least a file or directory needs to be specified
		MSG_INFO(OLTFSC0019E);
		goto error;
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
