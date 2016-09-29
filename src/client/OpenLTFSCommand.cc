#include <unistd.h>
#include <string>
#include <fstream>

#include "src/common/messages/Message.h"
#include "src/common/tracing/Trace.h"
#include "src/common/errors/errors.h"

#include "src/common/comm/ltfsdm.pb.h"
#include "src/common/comm/LTFSDmComm.h"

#include "OpenLTFSCommand.h"


void OpenLTFSCommand::processOptions(int argc, char **argv)

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
				INFO(LTFSDMC0014E);
				printUsage();
				throw(LTFSDMErr::LTFSDM_GENERAL_ERROR);
			default:
				INFO(LTFSDMC0013E);
				printUsage();
				throw(LTFSDMErr::LTFSDM_GENERAL_ERROR);
		}
	}
}

void OpenLTFSCommand::getRequestNumber()

{
	/* COMMAND WILL BE REQNUMBER */
	LTFSDmProtocol::LTFSDmReqNumber *reqnum = commCommand.mutable_reqnum();
	reqnum->set_key(key);

	try {
		commCommand.send();
	}
	catch(...) {
		MSG(LTFSDMC0027E);
		throw(LTFSDMErr::LTFSDM_GENERAL_ERROR);
	}

	try {
		commCommand.recv();
	}
	catch(...) {
		MSG(LTFSDMC0027E);
		throw(LTFSDMErr::LTFSDM_GENERAL_ERROR);
	}

	const LTFSDmProtocol::LTFSDmReqNumberResp reqnumresp = commCommand.reqnumresp();

	if( reqnumresp.success() == true ) {
		requestNumber = reqnumresp.reqnumber();
		TRACE(Trace::little, requestNumber);
	}
	else {
		MSG(LTFSDMC0029E);
		throw(LTFSDMErr::LTFSDM_GENERAL_ERROR);
	}
}

void OpenLTFSCommand::connect()

{
	std::ifstream keyFile;
	std::string line;

    keyFile.exceptions(std::ofstream::failbit | std::ofstream::badbit);

    try {
        keyFile.open(Const::KEY_FILE);
		std::getline(keyFile, line);
		key = std::stol(line);
    }
    catch(...) {
		TRACE(Trace::error, key);
		MSG(LTFSDMC0025E);
		throw(LTFSDMErr::LTFSDM_GENERAL_ERROR);
    }

	keyFile.close();

	try {
		commCommand.connect();
	}
	catch(...) {
		MSG(LTFSDMC0026E);
		throw(LTFSDMErr::LTFSDM_GENERAL_ERROR);
	}

	if ( requestNumber == Const::UNSET )
		getRequestNumber();

	TRACE(Trace::little, requestNumber);
}
