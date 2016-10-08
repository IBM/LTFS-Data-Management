#include <unistd.h>
#include <string>
#include <fstream>

#include "src/common/messages/Message.h"
#include "src/common/tracing/Trace.h"
#include "src/common/errors/errors.h"

#include "src/common/comm/ltfsdm.pb.h"
#include "src/common/comm/LTFSDmComm.h"

#include "OpenLTFSCommand.h"

OpenLTFSCommand::~OpenLTFSCommand()

{
	std::ifstream filelist;

	if ( fileListStrm.is_open() ) {
		fileListStrm.close();
	}
}

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

void OpenLTFSCommand::traceParms()

{
	TRACE(Trace::little, waitForCompletion);
	TRACE(Trace::little, preMigrate);
	TRACE(Trace::little, recToResident);
	TRACE(Trace::little, requestNumber);
	TRACE(Trace::little, collocationFactor);
	TRACE(Trace::little, fileList);
	TRACE(Trace::little, directoryName);
	TRACE(Trace::little, command);
	TRACE(Trace::little, optionStr);
	TRACE(Trace::little, key);
}

void OpenLTFSCommand::getRequestNumber()

{
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
		MSG(LTFSDMC0028E);
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
		throw(LTFSDMErr::LTFSDM_GENERAL_ERROR);
	}

	if ( requestNumber == Const::UNSET )
		getRequestNumber();

	TRACE(Trace::little, requestNumber);
}


void OpenLTFSCommand::checkOptions(int argc, char **argv)

{
	if ( fileList.compare("") && directoryName.compare("") ) {
		INFO(LTFSDMC0015E);
		MSG(LTFSDMC0029E);
		throw(LTFSDMErr::LTFSDM_GENERAL_ERROR);

	}

	if (optind != argc) {
		if (fileList.compare("")) {
			INFO(LTFSDMC0016E);
			MSG(LTFSDMC0029E);
			throw(LTFSDMErr::LTFSDM_GENERAL_ERROR);

		}
		if (directoryName.compare("")) {
			INFO(LTFSDMC0017E);
			MSG(LTFSDMC0029E);
			throw(LTFSDMErr::LTFSDM_GENERAL_ERROR);

		}
	}
}


void OpenLTFSCommand::sendObjects(std::stringstream *parmList)

{
	std::istream *input;
	std::string line;
	bool cont = true;
	int i;

	if ( fileList.compare("") ) {
		fileListStrm.open(fileList);
		input =  dynamic_cast<std::istream*>(&fileListStrm);
	}
	else {
		input = dynamic_cast<std::istream*>(parmList);
	}

	while (cont) {
		LTFSDmProtocol::LTFSDmSendObjects *sendobjects = commCommand.mutable_sendobjects();
		LTFSDmProtocol::LTFSDmSendObjects::FileName* filenames;

		for ( i = 0; (i < Const::MAX_OBJECTS_SEND) && ((std::getline(*input, line))); i++ ) {
			filenames = sendobjects->add_filenames();
			filenames->set_filename(line);
			TRACE(Trace::much, line);
		}

		if ( i < Const::MAX_OBJECTS_SEND ) {
			cont = false;
			filenames = sendobjects->add_filenames();
			filenames->set_filename(""); //end
			TRACE(Trace::much, "END");
		}

		try {
			commCommand.send();
		}
		catch(...) {
			MSG(LTFSDMC0027E);
			throw LTFSDMErr::LTFSDM_GENERAL_ERROR;
		}

		try {
			commCommand.recv();
		}
		catch(...) {
			MSG(LTFSDMC0028E);
			throw(LTFSDMErr::LTFSDM_GENERAL_ERROR);
		}

		if ( ! commCommand.has_sendobjectsresp() ) {
			MSG(LTFSDMC0039E);
			throw(LTFSDMErr::LTFSDM_GENERAL_ERROR);
		}

		const LTFSDmProtocol::LTFSDmSendObjectsResp sendobjresp = commCommand.sendobjectsresp();

		if( sendobjresp.success() == true ) {
			if ( getpid() != sendobjresp.pid() ) {
				MSG(LTFSDMC0036E);
				TRACE(Trace::error, getpid());
				TRACE(Trace::error, sendobjresp.pid());
				throw(LTFSDMErr::LTFSDM_GENERAL_ERROR);
			}
			if ( requestNumber != sendobjresp.reqnumber() ) {
				MSG(LTFSDMC0037E);
				TRACE(Trace::error, requestNumber);
				TRACE(Trace::error, sendobjresp.reqnumber());
				throw(LTFSDMErr::LTFSDM_GENERAL_ERROR);
			}
		}
		else {
			MSG(LTFSDMC0029E);
			throw(LTFSDMErr::LTFSDM_GENERAL_ERROR);
		}
	}
}
