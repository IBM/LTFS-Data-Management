#include <unistd.h>
#include <string>

#include "src/common/messages/messages.h"
#include "src/common/tracing/trace.h"
#include "src/common/errors/errors.h"

#include "Operation.h"


void Operation::processOptions(int argc, char **argv)

{
	int opt;

	opterr = 0;

	while (( opt = getopt(argc, argv, optionStr.c_str())) != -1 ) {
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
				recToResident = true;
				break;
			case 'n':
				requestNumber = strtoul(optarg, NULL, 0);;
				break;
			case 'c':
				collocationFactor = strtoul(optarg, NULL, 0);;
				break;
			case 'f':
				fileList = std::string(optarg);
				break;
			case 'R':
				directoryName = std::string(optarg);
				break;
			case ':':
				MSG_INFO(OLTFSC0014E);
				printUsage();
				throw(OLTFSErr::OLTFS_GENERAL_ERROR);
			default:
				MSG_INFO(OLTFSC0013E);
				printUsage();
				throw(OLTFSErr::OLTFS_GENERAL_ERROR);
		}
	}
}
