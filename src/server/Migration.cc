#include <iostream>
#include <vector>

#include "src/common/comm/ltfsdm.pb.h"

#include "FileOperation.h"
#include "Migration.h"


void Migration::start()

{
	int i = 0;

	std::cout << "Migration request" << std::endl;

	std::cout << "pid: " << pid << std::endl;
	std::cout << "request number: " << reqNumber << std::endl;
	std::cout << "colocation factor: " << colFactor << std::endl;

	switch( targetState ) {
		case LTFSDmProtocol::LTFSDmMigRequest::MIGRATED:
			std::cout << "files to be migrated" << std::endl;
			break;
		case LTFSDmProtocol::LTFSDmMigRequest::PREMIGRATED:
			std::cout << "files to be premigrated" << std::endl;
			break;
		default:
			std::cout << "unkown target state" << std::endl;
	}

	for(std::vector<std::string>::iterator it = fileList.begin(); it != fileList.end(); ++it) {
		std::cout << "file " << ++i << ": " << *it << std::endl;
	}

	std::cout << std::endl;
}
