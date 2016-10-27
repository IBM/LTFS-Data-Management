#include <unistd.h>
#include <string>
#include <sstream>

#include "src/common/messages/Message.h"
#include "src/common/tracing/Trace.h"
#include "src/common/errors/errors.h"

#include "src/common/comm/ltfsdm.pb.h"
#include "src/common/comm/LTFSDmComm.h"

#include "OpenLTFSCommand.h"
#include "MigrationCommand.h"

void MigrationCommand::printUsage()
{
	INFO(LTFSDMC0001I);
}


void MigrationCommand::talkToBackend(std::stringstream *parmList)

{
	try {
		connect();
	}
	catch (...) {
		MSG(LTFSDMC0026E);
		return;
	}

	LTFSDmProtocol::LTFSDmMigRequest *migreq = commCommand.mutable_migrequest();

	migreq->set_key(key);
	migreq->set_reqnumber(requestNumber);
	migreq->set_pid(getpid());
	migreq->set_colfactor(collocationFactor);


	if ( preMigrate == true )
		migreq->set_state(LTFSDmProtocol::LTFSDmMigRequest::PREMIGRATED);
	else
		migreq->set_state(LTFSDmProtocol::LTFSDmMigRequest::MIGRATED);

	if (!directoryName.compare(""))
		migreq->set_directory(true);
	else
		migreq->set_directory(false);

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

	const LTFSDmProtocol::LTFSDmMigRequestResp migreqresp = commCommand.migrequestresp();

	if( migreqresp.success() == true ) {
		if ( getpid() != migreqresp.pid() ) {
			MSG(LTFSDMC0036E);
			TRACE(Trace::error, getpid());
			TRACE(Trace::error, migreqresp.pid());
			throw(Error::LTFSDM_GENERAL_ERROR);
		}
		if ( requestNumber !=  migreqresp.reqnumber() ) {
			MSG(LTFSDMC0037E);
			TRACE(Trace::error, requestNumber);
			TRACE(Trace::error, migreqresp.reqnumber());
			throw(Error::LTFSDM_GENERAL_ERROR);
		}
	}
	else {
		MSG(LTFSDMC0029E);
		throw(Error::LTFSDM_GENERAL_ERROR);
	}

	sendObjects(parmList);

	queryResults();
}

void MigrationCommand::doCommand(int argc, char **argv)
{
	std::stringstream parmList;

	if ( argc == 1 ) {
		INFO(LTFSDMC0018E);
		throw(Error::LTFSDM_GENERAL_ERROR);

	}

	processOptions(argc, argv);

	checkOptions(argc, argv);

	TRACE(Trace::little, argc);
	TRACE(Trace::little, optind);
	traceParms();

	if ( !fileList.compare("") && !directoryName.compare("") ) {
		for ( int i=optind; i<argc; i++ ) {
			parmList << argv[i] << std::endl;
		}
	}
	else if ( directoryName.compare("") ) {
		parmList << directoryName << std::endl;
	}

	isValidRegularFile();

	talkToBackend(&parmList);
}
