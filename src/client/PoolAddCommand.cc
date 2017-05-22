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
#include "PoolAddCommand.h"

void PoolAddCommand::printUsage()
{
	INFO(LTFSDMC0077I);
}

void PoolAddCommand::doCommand(int argc, char **argv)
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

	LTFSDmProtocol::LTFSDmPoolAddRequest *pooladdreq = commCommand.mutable_pooladdrequest();
	pooladdreq->set_key(key);
	pooladdreq->set_poolname(poolNames);

	for ( std::string tapeid : tapeList )
		pooladdreq->add_tapeid(tapeid);

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
				INFO(LTFSDMC0083I, tapeid, poolNames);
				break;
			case Error::LTFSDM_POOL_NOT_EXISTS:
				MSG(LTFSDMX0025E, poolNames);
				break;
			case Error::LTFSDM_TAPE_NOT_EXISTS:
				MSG(LTFSDMC0084E,tapeid);
				break;
			case Error::LTFSDM_TAPE_EXISTS_IN_POOL:
				MSG(LTFSDMX0021E, tapeid);
				break;
			default:
				MSG(LTFSDMC0085E, tapeid, poolNames);
		}
	}
}
