#include <unistd.h>
#include <string>

#include "src/common/messages/Message.h"
#include "src/common/tracing/Trace.h"
#include "src/common/errors/errors.h"

#include "src/common/comm/ltfsdm.pb.h"
#include "src/common/comm/LTFSDmComm.h"

#include "OpenLTFSCommand.h"
#include "InfoFilesCommand.h"

void InfoFilesCommand::printUsage()
{
	INFO(LTFSDMC0010I);
}

void InfoFilesCommand::talkToBackend(std::stringstream *parmList)

{
	try {
		connect();
	}
	catch (...) {
		MSG(LTFSDMC0026E);
		return;
	}

	LTFSDmProtocol::LTFSDmInfoFilesRequest *infofilesreq = commCommand.mutable_infofilesrequest();

	infofilesreq->set_key(key);
	infofilesreq->set_reqnumber(requestNumber);
	infofilesreq->set_pid(getpid());

	if (!directoryName.compare(""))
		infofilesreq->set_directory(true);
	else
		infofilesreq->set_directory(false);

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

	const LTFSDmProtocol::LTFSDmInfoFilesRequestResp infofilesreqresp = commCommand.infofilesrequestresp();

	if( infofilesreqresp.success() == true ) {
		if ( getpid() != infofilesreqresp.pid() ) {
			MSG(LTFSDMC0036E);
			TRACE(Trace::error, getpid());
			TRACE(Trace::error, infofilesreqresp.pid());
			throw(Error::LTFSDM_GENERAL_ERROR);
		}
		if ( requestNumber !=  infofilesreqresp.reqnumber() ) {
			MSG(LTFSDMC0037E);
			TRACE(Trace::error, requestNumber);
			TRACE(Trace::error, infofilesreqresp.reqnumber());
			throw(Error::LTFSDM_GENERAL_ERROR);
		}
	}

	else {
		MSG(LTFSDMC0029E);
		throw(Error::LTFSDM_GENERAL_ERROR);
	}

	sendObjects(parmList);
}

void InfoFilesCommand::doCommand(int argc, char **argv)
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

	if ( !fileList.compare("") && !directoryName.compare("") ) {
		for ( int i=optind; i<argc; i++ ) {
			parmList << argv[i] << std::endl;
		}
	}
	else if ( directoryName.compare("") ) {
		parmList << directoryName << std::endl;
	}

	isValidRegularFile();

	talkToBackend(&parmList);
}
