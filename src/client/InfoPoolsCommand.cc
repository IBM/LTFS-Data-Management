#include <sys/resource.h>

#include <unistd.h>
#include <string>
#include <list>

#include "src/common/messages/Message.h"
#include "src/common/tracing/Trace.h"
#include "src/common/errors/errors.h"

#include "src/common/comm/ltfsdm.pb.h"
#include "src/common/comm/LTFSDmComm.h"

#include "OpenLTFSCommand.h"
#include "InfoPoolsCommand.h"

void InfoPoolsCommand::printUsage()
{
	INFO(LTFSDMC0087I);
}

void InfoPoolsCommand::doCommand(int argc, char **argv)
{
	processOptions(argc, argv);

	TRACE(Trace::little, *argv);
	TRACE(Trace::little, argc);
	TRACE(Trace::little, optind);

	if( argc != optind ) {
		printUsage();
		throw(Error::LTFSDM_GENERAL_ERROR);
	}

	try {
		connect();
	}
	catch (...) {
		MSG(LTFSDMC0026E);
		return;
	}

	LTFSDmProtocol::LTFSDmInfoPoolsRequest *infopools = commCommand.mutable_infopoolsrequest();

	infopools->set_key(key);

	try {
		commCommand.send();
	}
	catch(...) {
		MSG(LTFSDMC0027E);
		throw Error::LTFSDM_GENERAL_ERROR;
	}

	INFO(LTFSDMC0088I);

	std::string name;

	do {
		try {
			commCommand.recv();
		}
		catch(...) {
			MSG(LTFSDMC0028E);
			throw(Error::LTFSDM_GENERAL_ERROR);
		}

		const LTFSDmProtocol::LTFSDmInfoPoolsResp infopoolsresp = commCommand.infopoolsresp();
		name = infopoolsresp.poolname();
		unsigned long total = infopoolsresp.total();
		unsigned long free = infopoolsresp.free();
		unsigned long unref = infopoolsresp.unref();
		unsigned long numtapes = infopoolsresp.numtapes();
		if ( name.compare("") != 0 )
			INFO(LTFSDMC0089I, name, total, free, unref, numtapes);
	} while ( name.compare("") != 0 );

	return;
}
