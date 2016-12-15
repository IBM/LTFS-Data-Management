#include <fcntl.h>
#include <sys/file.h>

#include <string>
#include <set>

#include "src/common/util/util.h"
#include "src/common/messages/Message.h"
#include "src/common/tracing/Trace.h"
#include "src/common/errors/errors.h"

#include "src/common/comm/ltfsdm.pb.h"
#include "src/common/comm/LTFSDmComm.h"

#include "OpenLTFSCommand.h"
#include "AddCommand.h"

void AddCommand::printUsage()
{
	INFO(LTFSDMC0052I);
}

void AddCommand::doCommand(int argc, char **argv)
{
	char *pathName = NULL;
	std::string mountPoint;

	if ( argc != 2 ) {
		printUsage();
		throw Error::LTFSDM_GENERAL_ERROR;
	}

	pathName = canonicalize_file_name(argv[1]);

	if ( pathName == NULL ) {
		MSG(LTFSDMC0053E);
		throw Error::LTFSDM_GENERAL_ERROR;
	}

	mountPoint = std::string(pathName);

	if ( LTFSDM::getFs().count(mountPoint) == 0 ) {
		MSG(LTFSDMC0053E);
		throw Error::LTFSDM_GENERAL_ERROR;
	}

	try {
		connect();
	}
	catch(...) {
		MSG(LTFSDMC0026E);
		throw(Error::LTFSDM_GENERAL_ERROR);
	}

	TRACE(Trace::error, requestNumber);

	LTFSDmProtocol::LTFSDmAddRequest *addreq = commCommand.mutable_addrequest();
	addreq->set_key(key);
	addreq->set_reqnumber(requestNumber);
	addreq->set_mountpoint(mountPoint);

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

	const LTFSDmProtocol::LTFSDmAddResp addresp = commCommand.addresp();

	if( addresp.success() != true ) {
		MSG(LTFSDMC0029E);
		throw(Error::LTFSDM_GENERAL_ERROR);
	}
}
