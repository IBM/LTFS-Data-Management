#include <iostream>
#include <vector>

#include "src/common/comm/ltfsdm.pb.h"

#include "FileOperation.h"

#include "SelRecall.h"

void SelRecall::start()

{
	int i = 0;

	std::cout << "Selective Recall request" << std::endl;

	std::cout << "pid: " << pid << std::endl;
	std::cout << "request number: " << reqNumber << std::endl;

	switch (targetState) {
		case LTFSDmProtocol::LTFSDmSelRecRequest::RESIDENT:
			std::cout << "files to be recalled to resident state" << std::endl;
			break;
		case LTFSDmProtocol::LTFSDmSelRecRequest::PREMIGRATED:
			std::cout << "files to be recalled to premigrated state" << std::endl;
			break;
		default:
			std::cout << "unkown target state" << std::endl;
	}

	for(std::vector<std::string>::iterator it = fileList.begin(); it != fileList.end(); ++it) {
		std::cout << "file " << ++i << ": " << *it << std::endl;
	}

	std::cout << std::endl;

}
