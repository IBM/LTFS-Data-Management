#include <fcntl.h>
#include <sys/file.h>
#include <sys/resource.h>

#include <string>
#include <set>
#include <vector>
#include <list>
#include <algorithm>

#include "src/common/util/util.h"
#include "src/common/messages/Message.h"
#include "src/common/tracing/Trace.h"
#include "src/common/errors/errors.h"

#include "src/common/comm/ltfsdm.pb.h"
#include "src/common/comm/LTFSDmComm.h"

#include "OpenLTFSCommand.h"
#include "PoolCreateCommand.h"

void PoolCreateCommand::printUsage()
{
	INFO(LTFSDMC0075I);
}

void PoolCreateCommand::doCommand(int argc, char **argv)
{
	if ( argc <= 2 ) {
		printUsage();
		throw Error::LTFSDM_GENERAL_ERROR;
	}

	processOptions(argc, argv);

	if ( argc != optind ) {
		printUsage();
		throw Error::LTFSDM_GENERAL_ERROR;
	}

	if ( std::count(poolName.begin(), poolName.end(), 10) > 0 ) {
		MSG(LTFSDMC0091E);
		throw Error::LTFSDM_GENERAL_ERROR;
	}

	try {
		connect();
	}
	catch(...) {
		MSG(LTFSDMC0026E);
		throw(Error::LTFSDM_GENERAL_ERROR);
	}

	LTFSDmProtocol::LTFSDmPoolCreateRequest *poolcreatereq = commCommand.mutable_poolcreaterequest();
	poolcreatereq->set_key(key);
	poolcreatereq->set_poolname(poolName);

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

	const LTFSDmProtocol::LTFSDmPoolResp poolresp = commCommand.poolresp();

	switch ( poolresp.response() ) {
		case Error::LTFSDM_OK:
			INFO(LTFSDMC0079I, poolName);
			break;
		case Error::LTFSDM_POOL_EXISTS:
			MSG(LTFSDMX0023E, poolName);
			break;
		default:
			MSG(LTFSDMC0080E, poolName);
	}
}
