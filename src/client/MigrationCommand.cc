#include <unistd.h>
#include <sys/resource.h>

#include <string>
#include <sstream>
#include <list>
#include <exception>

#include "src/common/exception/OpenLTFSException.h"
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
	catch (const std::exception& e) {
		MSG(LTFSDMC0026E);
		return;
	}

	LTFSDmProtocol::LTFSDmMigRequest *migreq = commCommand.mutable_migrequest();

	migreq->set_key(key);
	migreq->set_reqnumber(requestNumber);
	migreq->set_pid(getpid());
	migreq->set_pools(poolNames);

	if ( preMigrate == true )
		migreq->set_state(LTFSDmProtocol::LTFSDmMigRequest::PREMIGRATED);
	else
		migreq->set_state(LTFSDmProtocol::LTFSDmMigRequest::MIGRATED);

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

	const LTFSDmProtocol::LTFSDmMigRequestResp migreqresp = commCommand.migrequestresp();

	switch ( migreqresp.error() ) {
		case Error::LTFSDM_OK:
			if ( getpid() != migreqresp.pid() ) {
				MSG(LTFSDMC0036E);
				TRACE(Trace::error, getpid(), migreqresp.pid());
				throw(EXCEPTION(Error::LTFSDM_GENERAL_ERROR));
			}
			if ( requestNumber !=  migreqresp.reqnumber() ) {
				MSG(LTFSDMC0037E);
				TRACE(Trace::error, requestNumber, migreqresp.reqnumber());
				throw(EXCEPTION(Error::LTFSDM_GENERAL_ERROR));
			}
			break;
		case Error::LTFSDM_WRONG_POOLNUM:
			MSG(LTFSDMS0063E);
			throw(EXCEPTION(Error::LTFSDM_GENERAL_ERROR));
			break;
		case Error::LTFSDM_NOT_ALL_POOLS_EXIST:
			MSG(LTFSDMS0064E);
			throw(EXCEPTION(Error::LTFSDM_GENERAL_ERROR));
			break;
		case Error::LTFSDM_TERMINATING:
			MSG(LTFSDMC0101I);
			throw(EXCEPTION(Error::LTFSDM_GENERAL_ERROR));
			break;
		default:
			MSG(LTFSDMC0029E);
			throw(EXCEPTION(Error::LTFSDM_GENERAL_ERROR));
	}

	sendObjects(parmList);

	queryResults();
}

void MigrationCommand::doCommand(int argc, char **argv)
{
	std::stringstream parmList;

	if ( argc == 1 ) {
		INFO(LTFSDMC0018E);
		throw(EXCEPTION(Error::LTFSDM_GENERAL_ERROR));
	}

	processOptions(argc, argv);

	try {
		checkOptions(argc, argv);
	}
	catch (const std::exception& e) {
		printUsage();
		throw(EXCEPTION(Error::LTFSDM_GENERAL_ERROR));
	}

	TRACE(Trace::normal, argc, optind);
	traceParms();

	if ( !fileList.compare("") ) {
		for ( int i=optind; i<argc; i++ ) {
			parmList << argv[i] << std::endl;
		}
	}

	isValidRegularFile();

	talkToBackend(&parmList);
}
