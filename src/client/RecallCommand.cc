#include <unistd.h>
#include <sys/resource.h>

#include <string>

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
	catch (...) {
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

	const LTFSDmProtocol::LTFSDmSelRecRequestResp recreqresp = commCommand.selrecrequestresp();

	if( recreqresp.success() == true ) {
		if ( getpid() != recreqresp.pid() ) {
			MSG(LTFSDMC0036E);
			TRACE(Trace::error, getpid());
			TRACE(Trace::error, recreqresp.pid());
			throw(Error::LTFSDM_GENERAL_ERROR);
		}
		if ( requestNumber !=  recreqresp.reqnumber() ) {
			MSG(LTFSDMC0037E);
			TRACE(Trace::error, requestNumber);
			TRACE(Trace::error, recreqresp.reqnumber());
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

void RecallCommand::doCommand(int argc, char **argv)
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

	if ( !fileList.compare("") ) {
		for ( int i=optind; i<argc; i++ ) {
			parmList << argv[i] << std::endl;
		}
	}

	isValidRegularFile();

	talkToBackend(&parmList);
}
