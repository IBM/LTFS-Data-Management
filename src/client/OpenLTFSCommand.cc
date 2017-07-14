#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <sys/resource.h>
#include <string>
#include <fstream>
#include <list>
#include <sstream>
#include <exception>

#include "src/common/exception/OpenLTFSException.h"
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
				throw(EXCEPTION(Error::LTFSDM_OK));
			case 'p':
				preMigrate = true;
				break;
			case 'r':
				recToResident = true;
				break;
			case 'n':
				requestNumber = strtoul(optarg, NULL, 0);
				if ( requestNumber < 0 ) {
					MSG(LTFSDMC0064E);
					printUsage();
					throw(EXCEPTION(Error::LTFSDM_GENERAL_ERROR));
				}
				break;
			case 'f':
				fileList = std::string(optarg);
				break;
			case 'm':
				mountPoint = std::string(optarg);
				break;
			case 'N':
				fsName = std::string(optarg);
				break;
			case 'P':
				poolNames = std::string(optarg);
				break;
			case 't':
				tapeList.push_back(std::string(optarg));
				break;
			case 'x':
				forced = true;
				break;
			case ':':
				INFO(LTFSDMC0014E);
				printUsage();
				throw(EXCEPTION(Error::LTFSDM_GENERAL_ERROR));
			default:
				INFO(LTFSDMC0013E);
				printUsage();
				throw(EXCEPTION(Error::LTFSDM_GENERAL_ERROR));
		}
	}
}

void OpenLTFSCommand::traceParms()

{
	TRACE(Trace::normal, preMigrate);
	TRACE(Trace::normal, recToResident);
	TRACE(Trace::normal, requestNumber);
	TRACE(Trace::normal, poolNames);
	TRACE(Trace::normal, fileList);
	TRACE(Trace::normal, command);
	TRACE(Trace::normal, optionStr);
	TRACE(Trace::normal, key);
}

void OpenLTFSCommand::getRequestNumber()

{
	LTFSDmProtocol::LTFSDmReqNumber *reqnum = commCommand.mutable_reqnum();
	reqnum->set_key(key);

	try {
		commCommand.send();
	}
	catch(const std::exception& e) {
		MSG(LTFSDMC0027E);
		throw(EXCEPTION(Error::LTFSDM_GENERAL_ERROR));
	}

	try {
		commCommand.recv();
	}
	catch(const std::exception& e) {
		MSG(LTFSDMC0028E);
		throw(EXCEPTION(Error::LTFSDM_GENERAL_ERROR));
	}

	const LTFSDmProtocol::LTFSDmReqNumberResp reqnumresp = commCommand.reqnumresp();

	if( reqnumresp.success() == true ) {
		requestNumber = reqnumresp.reqnumber();
		TRACE(Trace::normal, requestNumber);
	}
	else {
		MSG(LTFSDMC0029E);
		throw(EXCEPTION(Error::LTFSDM_GENERAL_ERROR));
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
    catch(const std::exception& e) {
		TRACE(Trace::error, key);
		MSG(LTFSDMC0025E);
		throw(EXCEPTION(Error::LTFSDM_GENERAL_ERROR));
    }

	keyFile.close();

	try {
		commCommand.connect();
	}
	catch(const std::exception& e) {
		throw(EXCEPTION(Error::LTFSDM_GENERAL_ERROR));
	}

	if ( requestNumber == Const::UNSET )
		getRequestNumber();

	TRACE(Trace::normal, requestNumber);
}


void OpenLTFSCommand::checkOptions(int argc, char **argv)

{
	if ( (requestNumber != Const::UNSET) && (argc != 3) )
		throw(EXCEPTION(Error::LTFSDM_GENERAL_ERROR));

	if (optind != argc) {
		if (fileList.compare("")) {
			INFO(LTFSDMC0016E);
			throw(EXCEPTION(Error::LTFSDM_GENERAL_ERROR));
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
			TRACE(Trace::full, line);
		}

		if ( i < Const::MAX_OBJECTS_SEND ) {
			cont = false;
			filenames = sendobjects->add_filenames();
			filenames->set_filename(""); //end
			TRACE(Trace::full, "END");
		}

		try {
			commCommand.send();
		}
		catch(const std::exception& e) {
			MSG(LTFSDMC0027E);
			throw(EXCEPTION(Error::LTFSDM_GENERAL_ERROR));
		}

		try {
			commCommand.recv();
		}
		catch(const std::exception& e) {
			MSG(LTFSDMC0028E);
			throw(EXCEPTION(Error::LTFSDM_GENERAL_ERROR));
		}

		if ( ! commCommand.has_sendobjectsresp() ) {
			MSG(LTFSDMC0039E);
			throw(EXCEPTION(Error::LTFSDM_GENERAL_ERROR));
		}

		const LTFSDmProtocol::LTFSDmSendObjectsResp sendobjresp = commCommand.sendobjectsresp();

		if( sendobjresp.success() == true ) {
			if ( getpid() != sendobjresp.pid() ) {
				MSG(LTFSDMC0036E);
				TRACE(Trace::error, getpid());
				TRACE(Trace::error, sendobjresp.pid());
				throw(EXCEPTION(Error::LTFSDM_GENERAL_ERROR));
			}
			if ( requestNumber != sendobjresp.reqnumber() ) {
				MSG(LTFSDMC0037E);
				TRACE(Trace::error, requestNumber);
				TRACE(Trace::error, sendobjresp.reqnumber());
				throw(EXCEPTION(Error::LTFSDM_GENERAL_ERROR));
			}
		}
		else {
			MSG(LTFSDMC0029E);
			throw(EXCEPTION(Error::LTFSDM_GENERAL_ERROR));
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
	long failed = 0;
	bool first = true;
	bool done = false;
	time_t duration;
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
		catch(const std::exception& e) {
			MSG(LTFSDMC0027E);
			throw(EXCEPTION(Error::LTFSDM_GENERAL_ERROR));
		}

		try {
			commCommand.recv();
		}
		catch(const std::exception& e) {
			MSG(LTFSDMC0028E);
			throw(EXCEPTION(Error::LTFSDM_GENERAL_ERROR));
		}

		const LTFSDmProtocol::LTFSDmReqStatusResp reqstatusresp = commCommand.reqstatusresp();

		if( reqstatusresp.success() == true ) {
			if ( getpid() != reqstatusresp.pid() ) {
				MSG(LTFSDMC0036E);
				TRACE(Trace::error, getpid());
				TRACE(Trace::error, reqstatusresp.pid());
				throw(EXCEPTION(Error::LTFSDM_GENERAL_ERROR));
			}
			if ( requestNumber !=  reqstatusresp.reqnumber() ) {
				MSG(LTFSDMC0037E);
				TRACE(Trace::error, requestNumber);
				TRACE(Trace::error, reqstatusresp.reqnumber());
				throw(EXCEPTION(Error::LTFSDM_GENERAL_ERROR));
			}
			resident =  reqstatusresp.resident();
			premigrated =  reqstatusresp.premigrated();
			migrated =  reqstatusresp.migrated();
			failed = reqstatusresp.failed();
			done = reqstatusresp.done();

			duration = time(NULL) - startTime;
			gmtime_r(&duration, &tmval);
			strftime(curctime, sizeof(curctime) - 1, "%H:%M:%S", &tmval);
			if ( first ) {
				INFO(LTFSDMC0046I);
				first = false;
			}
			INFO(LTFSDMC0045I, curctime, resident, premigrated, migrated, failed);
		}
		else {
			MSG(LTFSDMC0029E);
			throw(EXCEPTION(Error::LTFSDM_GENERAL_ERROR));
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
			throw(EXCEPTION(Error::LTFSDM_GENERAL_ERROR));
		}
		if ( !S_ISREG(statbuf.st_mode) ) {
			MSG(LTFSDMC0042E, fileList.c_str());
			throw(EXCEPTION(Error::LTFSDM_GENERAL_ERROR));
		}
		if ( statbuf.st_size < 2 ) {
			MSG(LTFSDMC0041E, fileList.c_str());
			throw(EXCEPTION(Error::LTFSDM_GENERAL_ERROR));
		}
	}
}
