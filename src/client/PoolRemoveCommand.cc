#include <fcntl.h>
#include <sys/file.h>
#include <sys/resource.h>

#include <string>
#include <set>
#include <vector>
#include <list>

#include "src/common/util/util.h"
#include "src/common/messages/Message.h"
#include "src/common/tracing/Trace.h"
#include "src/common/errors/errors.h"

#include "src/common/comm/ltfsdm.pb.h"
#include "src/common/comm/LTFSDmComm.h"

#include "OpenLTFSCommand.h"
#include "PoolRemoveCommand.h"

void PoolRemoveCommand::printUsage()
{
	INFO(LTFSDMC0078I);
}

void PoolRemoveCommand::doCommand(int argc, char **argv)
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

	try {
		connect();
	}
	catch(...) {
		MSG(LTFSDMC0026E);
		throw(Error::LTFSDM_GENERAL_ERROR);
	}

	LTFSDmProtocol::LTFSDmPoolRemoveRequest *poolremovereq = commCommand.mutable_poolremoverequest();
	poolremovereq->set_key(key);
	poolremovereq->set_poolname(poolName);

	for ( std::string tapeid : tapeList )
		poolremovereq->add_tapeid(tapeid);


	try {
		commCommand.send();
	}
	catch(...) {
		MSG(LTFSDMC0027E);
		throw Error::LTFSDM_GENERAL_ERROR;
	}


	for ( unsigned int i = 0; i < tapeList.size(); i++ ) {

		try {
			commCommand.recv();
		}
		catch(...) {
			MSG(LTFSDMC0028E);
			throw(Error::LTFSDM_GENERAL_ERROR);
		}

		const LTFSDmProtocol::LTFSDmPoolResp poolresp = commCommand.poolresp();

		std::string tapeid = poolresp.tapeid();

		switch ( poolresp.response() ) {
			case Error::LTFSDM_OK:
				INFO(LTFSDMC0086I, tapeid, poolName);
				break;
			case Error::LTFSDM_POOL_NOT_EXISTS:
				MSG(LTFSDMX0025E, poolName);
				break;
			case Error::LTFSDM_TAPE_NOT_EXISTS:
				MSG(LTFSDMC0084E,tapeid);
				break;
			case Error::LTFSDM_TAPE_NOT_EXISTS_IN_POOL:
				MSG(LTFSDMX0022E, tapeid, poolName);
				break;
			default:
				MSG(LTFSDMC0085E, tapeid, poolName);
		}
	}
}
