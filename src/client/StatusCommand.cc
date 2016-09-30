#include <string>
#include "src/common/messages/Message.h"
#include "src/common/tracing/Trace.h"
#include "src/common/errors/errors.h"

#include "src/common/comm/ltfsdm.pb.h"
#include "src/common/comm/LTFSDmComm.h"

#include "OpenLTFSCommand.h"
#include "StatusCommand.h"

void StatusCommand::printUsage()
{
	INFO(LTFSDMC0030I);
}

void StatusCommand::doCommand(int argc, char **argv)
{
	int pid;

	if ( argc > 1 ) {
		printUsage();
		throw LTFSDMErr::LTFSDM_GENERAL_ERROR;
	}

	try {
		connect();
	}
	catch (...) {
		MSG(LTFSDMC0031I);
		return;
	}

	TRACE(Trace::error, requestNumber);

	LTFSDmProtocol::LTFSDmStatusRequest *statusreq = commCommand.mutable_statusrequest();
	statusreq->set_key(key);
	statusreq->set_reqnumber(requestNumber);

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

	const LTFSDmProtocol::LTFSDmStatusResp statusresp = commCommand.statusresp();

	if( statusresp.success() == true ) {
		pid = statusresp.pid();
		MSG(LTFSDMC0032I, pid);
	}
	else {
		MSG(LTFSDMC0029E);
		throw(LTFSDMErr::LTFSDM_GENERAL_ERROR);
	}
}
