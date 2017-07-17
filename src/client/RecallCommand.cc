#include <unistd.h>
#include <sys/resource.h>

#include <string>
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
#include "RecallCommand.h"

void RecallCommand::printUsage()
{
	INFO(LTFSDMC0002I);
}

void RecallCommand::talkToBackend(std::stringstream *parmList)

{
	try {
		connect();
	}
	catch (const std::exception& e) {
		MSG(LTFSDMC0026E);
		return;
	}

	LTFSDmProtocol::LTFSDmSelRecRequest *recreq = commCommand.mutable_selrecrequest();

	recreq->set_key(key);
	recreq->set_reqnumber(requestNumber);
	recreq->set_pid(getpid());

	if ( recToResident == true )
		recreq->set_state(LTFSDmProtocol::LTFSDmSelRecRequest::RESIDENT);
	else
		recreq->set_state(LTFSDmProtocol::LTFSDmSelRecRequest::PREMIGRATED);

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

	const LTFSDmProtocol::LTFSDmSelRecRequestResp recreqresp = commCommand.selrecrequestresp();

	switch ( recreqresp.error() ) {
		case Error::LTFSDM_OK:
			if ( getpid() != recreqresp.pid() ) {
				MSG(LTFSDMC0036E);
				TRACE(Trace::error, getpid());
				TRACE(Trace::error, recreqresp.pid());
				throw(EXCEPTION(Error::LTFSDM_GENERAL_ERROR));
			}
			if ( requestNumber !=  recreqresp.reqnumber() ) {
				MSG(LTFSDMC0037E);
				TRACE(Trace::error, requestNumber);
				TRACE(Trace::error, recreqresp.reqnumber());
				throw(EXCEPTION(Error::LTFSDM_GENERAL_ERROR));
			}
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

void RecallCommand::doCommand(int argc, char **argv)
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

	TRACE(Trace::normal, argc);
	TRACE(Trace::normal, optind);
	traceParms();

	if ( !fileList.compare("") ) {
		for ( int i=optind; i<argc; i++ ) {
			parmList << argv[i] << std::endl;
		}
	}

	isValidRegularFile();

	talkToBackend(&parmList);
}
