#include <sys/types.h>
#include <sys/stat.h>
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
				throw(Error::LTFSDM_OK);
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
				requestNumber = strtoul(optarg, NULL, 0);
				break;
			case 'c':
				collocationFactor = strtoul(optarg, NULL, 0);
				break;
			case 'f':
				fileList = std::string(optarg);
				break;
			case 'R':
				numReplica = strtoul(optarg, NULL, 0);
				break;
			case ':':
				INFO(LTFSDMC0014E);
				printUsage();
				throw(Error::LTFSDM_GENERAL_ERROR);
			default:
				INFO(LTFSDMC0013E);
				printUsage();
				throw(Error::LTFSDM_GENERAL_ERROR);
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
	TRACE(Trace::little, numReplica);
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
		throw(Error::LTFSDM_GENERAL_ERROR);
	}

	try {
		commCommand.recv();
	}
	catch(...) {
		MSG(LTFSDMC0028E);
		throw(Error::LTFSDM_GENERAL_ERROR);
	}

	const LTFSDmProtocol::LTFSDmReqNumberResp reqnumresp = commCommand.reqnumresp();

	if( reqnumresp.success() == true ) {
		requestNumber = reqnumresp.reqnumber();
		TRACE(Trace::little, requestNumber);
	}
	else {
		MSG(LTFSDMC0029E);
		throw(Error::LTFSDM_GENERAL_ERROR);
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
		throw(Error::LTFSDM_GENERAL_ERROR);
    }

	keyFile.close();

	try {
		commCommand.connect();
	}
	catch(...) {
		throw(Error::LTFSDM_GENERAL_ERROR);
	}

	if ( requestNumber == Const::UNSET )
		getRequestNumber();

	TRACE(Trace::little, requestNumber);
}


void OpenLTFSCommand::checkOptions(int argc, char **argv)

{
	if ( numReplica < 1 || numReplica > 3 ) {
			INFO(LTFSDMC0015E);
			throw(Error::LTFSDM_GENERAL_ERROR);
	}

	if (optind != argc) {
		if (fileList.compare("")) {
			INFO(LTFSDMC0016E);
			throw(Error::LTFSDM_GENERAL_ERROR);
		}
	}
}


void OpenLTFSCommand::sendObjects(std::stringstream *parmList)

{
	std::istream *input;
	std::string line;
	char *file_name;
	bool cont = true;
	int i;
	long startTime;
	unsigned int count = 0;

	startTime = time(NULL);

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
			file_name = canonicalize_file_name(line.c_str());
			if ( file_name ) {
				filenames = sendobjects->add_filenames();
				filenames->set_filename(file_name);
				free(file_name);
				count++;
			}
			else  {
				MSG(LTFSDMC0043E, line.c_str());
			}
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
			throw Error::LTFSDM_GENERAL_ERROR;
		}

		try {
			commCommand.recv();
		}
		catch(...) {
			MSG(LTFSDMC0028E);
			throw(Error::LTFSDM_GENERAL_ERROR);
		}

		if ( ! commCommand.has_sendobjectsresp() ) {
			MSG(LTFSDMC0039E);
			throw(Error::LTFSDM_GENERAL_ERROR);
		}

		const LTFSDmProtocol::LTFSDmSendObjectsResp sendobjresp = commCommand.sendobjectsresp();

		if( sendobjresp.success() == true ) {
			if ( getpid() != sendobjresp.pid() ) {
				MSG(LTFSDMC0036E);
				TRACE(Trace::error, getpid());
				TRACE(Trace::error, sendobjresp.pid());
				throw(Error::LTFSDM_GENERAL_ERROR);
			}
			if ( requestNumber != sendobjresp.reqnumber() ) {
				MSG(LTFSDMC0037E);
				TRACE(Trace::error, requestNumber);
				TRACE(Trace::error, sendobjresp.reqnumber());
				throw(Error::LTFSDM_GENERAL_ERROR);
			}
		}
		else {
			MSG(LTFSDMC0029E);
			throw(Error::LTFSDM_GENERAL_ERROR);
		}
		INFO(LTFSDMC0050I, count);
	}
	INFO(LTFSDMC0051I, time(NULL) - startTime);
}

void OpenLTFSCommand::queryResults()

{
	long resident = 0;
	long premigrated = 0;
	long migrated = 0;
	bool first = true;
	bool done = false;
	struct timeval curtime;
	struct tm tmval;;
	char curctime[26];

	do {
		LTFSDmProtocol::LTFSDmReqStatusRequest *reqstatus = commCommand.mutable_reqstatusrequest();

		reqstatus->set_key(key);
		reqstatus->set_reqnumber(requestNumber);
		reqstatus->set_pid(getpid());

		try {
			commCommand.send();
		}
		catch(...) {
			MSG(LTFSDMC0027E);
			throw Error::LTFSDM_GENERAL_ERROR;
		}

		try {
			commCommand.recv();
		}
		catch(...) {
			MSG(LTFSDMC0028E);
			throw(Error::LTFSDM_GENERAL_ERROR);
		}

		const LTFSDmProtocol::LTFSDmReqStatusResp reqstatusresp = commCommand.reqstatusresp();

		if( reqstatusresp.success() == true ) {
			if ( getpid() != reqstatusresp.pid() ) {
				MSG(LTFSDMC0036E);
				TRACE(Trace::error, getpid());
				TRACE(Trace::error, reqstatusresp.pid());
				throw(Error::LTFSDM_GENERAL_ERROR);
			}
			if ( requestNumber !=  reqstatusresp.reqnumber() ) {
				MSG(LTFSDMC0037E);
				TRACE(Trace::error, requestNumber);
				TRACE(Trace::error, reqstatusresp.reqnumber());
				throw(Error::LTFSDM_GENERAL_ERROR);
			}
			resident =  reqstatusresp.resident();
			premigrated =  reqstatusresp.premigrated();
			migrated =  reqstatusresp.migrated();
			done = reqstatusresp.done();

			gettimeofday(&curtime, NULL);
			localtime_r(&(curtime.tv_sec), &tmval);
			strftime(curctime, sizeof(curctime) - 1, "%H:%M:%S", &tmval);
			if ( first ) {
				INFO(LTFSDMC0046I);
				first = false;
			}
			INFO(LTFSDMC0045I, curctime, resident, premigrated, migrated);
		}
		else {
			MSG(LTFSDMC0029E);
			throw(Error::LTFSDM_GENERAL_ERROR);
		}
	} while (!done);
}


void OpenLTFSCommand::isValidRegularFile()

{
	struct stat statbuf;

	if ( !fileList.compare("-") ) { // if "-" is presented read from stdin
		fileList = "/dev/stdin";
	}
	else if ( fileList.compare("") ) {
		if ( stat(fileList.c_str(), &statbuf) ==  -1 ) {
			MSG(LTFSDMC0040E, fileList.c_str());
			throw(Error::LTFSDM_GENERAL_ERROR);
		}
		if ( !S_ISREG(statbuf.st_mode) ) {
			MSG(LTFSDMC0042E, fileList.c_str());
			throw(Error::LTFSDM_GENERAL_ERROR);
		}
		if ( statbuf.st_size < 2 ) {
			MSG(LTFSDMC0041E, fileList.c_str());
			throw(Error::LTFSDM_GENERAL_ERROR);
		}
	}
}
