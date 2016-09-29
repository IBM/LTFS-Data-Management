#include <string>
#include "src/common/messages/Message.h"
#include "src/common/tracing/Trace.h"
#include "src/common/errors/errors.h"

#include "src/common/comm/ltfsdm.pb.h"
#include "src/common/comm/LTFSDmComm.h"

#include "OpenLTFSCommand.h"
#include "StopCommand.h"

void StopCommand::printUsage()
{
	INFO(LTFSDMC0007I);
}

void StopCommand::doCommand(int argc, char **argv)
{

	if ( argc > 1 ) {
		printUsage();
		throw LTFSDMErr::LTFSDM_GENERAL_ERROR;
	}

	connect();

	TRACE(Trace::error, requestNumber);

	LTFSDmProtocol::LTFSDmStopRequest *stopreq = commCommand.mutable_stoprequest();
	stopreq->set_key(key);
	stopreq->set_reqnumber(requestNumber);

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
		MSG(LTFSDMC0027E);
		throw(LTFSDMErr::LTFSDM_GENERAL_ERROR);
	}

	const LTFSDmProtocol::LTFSDmStopResp stopresp = commCommand.stopresp();

	if( stopresp.success() != true ) {
		MSG(LTFSDMC0029E);
		throw(LTFSDMErr::LTFSDM_GENERAL_ERROR);
	}
}
