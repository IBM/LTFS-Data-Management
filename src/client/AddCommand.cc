#include <fcntl.h>
#include <sys/file.h>
#include <sys/resource.h>

#include <string>
#include <set>
#include <vector>

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
	std::string managedFs;

	if ( argc == 1 ) {
		printUsage();
		throw Error::LTFSDM_GENERAL_ERROR;
	}

	processOptions(argc, argv);

	pathName = canonicalize_file_name(argv[optind]);

	if ( pathName == NULL ) {
		MSG(LTFSDMC0053E);
		throw Error::LTFSDM_GENERAL_ERROR;
	}

	managedFs = std::string(pathName);

	if ( LTFSDM::getFs().count(managedFs) == 0 ) {
		MSG(LTFSDMC0053E);
		throw Error::LTFSDM_GENERAL_ERROR;
	}

	if ( argc != optind + 1 ) {
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

	TRACE(Trace::error, requestNumber);

	LTFSDmProtocol::LTFSDmAddRequest *addreq = commCommand.mutable_addrequest();
	addreq->set_key(key);
	addreq->set_reqnumber(requestNumber);
	addreq->set_managedfs(managedFs);
	addreq->set_mountpoint(mountPoint);
	addreq->set_fsname(fsName);

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

	if( addresp.response() == LTFSDmProtocol::LTFSDmAddResp::FAILED ) {
		MSG(LTFSDMC0055E, managedFs);
		throw(Error::LTFSDM_GENERAL_ERROR);
	}
	else if ( addresp.response() == LTFSDmProtocol::LTFSDmAddResp::ALREADY_ADDED ) {
		MSG(LTFSDMC0054I, managedFs);
	}
}
