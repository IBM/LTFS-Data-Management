#include <sys/resource.h>

#include <string>
#include <list>
#include "src/common/messages/Message.h"
#include "src/common/tracing/Trace.h"
#include "src/common/errors/errors.h"

#include "src/common/comm/ltfsdm.pb.h"
#include "src/common/comm/LTFSDmComm.h"

#include "OpenLTFSCommand.h"
#include "RetrieveCommand.h"

void RetrieveCommand::printUsage()
{
	INFO(LTFSDMC0093I);
}

void RetrieveCommand::doCommand(int argc, char **argv)
{
	processOptions(argc, argv);

	if ( argc > 1 ) {
		printUsage();
		throw Error::LTFSDM_GENERAL_ERROR;
	}

	try {
		connect();
	}
	catch (...) {
		MSG(LTFSDMC0026E);
		return;
	}

	LTFSDmProtocol::LTFSDmRetrieveRequest *retrievereq = commCommand.mutable_retrieverequest();
	retrievereq->set_key(key);

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

	const LTFSDmProtocol::LTFSDmRetrieveResp retrieveresp = commCommand.retrieveresp();

	if( retrieveresp.success() == false ) {
		MSG(LTFSDMC0094E);
		throw(Error::LTFSDM_GENERAL_ERROR);
	}
}
