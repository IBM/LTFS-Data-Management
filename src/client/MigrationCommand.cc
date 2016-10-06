#include <unistd.h>
#include <string>

#include "src/common/messages/Message.h"
#include "src/common/tracing/Trace.h"
#include "src/common/errors/errors.h"

#include "src/common/comm/ltfsdm.pb.h"
#include "src/common/comm/LTFSDmComm.h"

#include "OpenLTFSCommand.h"
#include "MigrationCommand.h"

void MigrationCommand::printUsage()
{
	INFO(LTFSDMC0001I);
}

void MigrationCommand::doCommand(int argc, char **argv)
{
	if ( argc == 1 ) {
		INFO(LTFSDMC0018E);
		MSG(LTFSDMC0029E);
		throw(LTFSDMErr::LTFSDM_GENERAL_ERROR);

	}

	processOptions(argc, argv);

	if ( fileList.compare("") && directoryName.compare("") ) {
		INFO(LTFSDMC0015E);
		MSG(LTFSDMC0029E);
		throw(LTFSDMErr::LTFSDM_GENERAL_ERROR);

	}

	if (optind != argc) {
		if (fileList.compare("")) {
			INFO(LTFSDMC0016E);
			MSG(LTFSDMC0029E);
			throw(LTFSDMErr::LTFSDM_GENERAL_ERROR);

		}
		if (directoryName.compare("")) {
			INFO(LTFSDMC0017E);
			MSG(LTFSDMC0029E);
			throw(LTFSDMErr::LTFSDM_GENERAL_ERROR);

		}
	}

	TRACE(Trace::little, argc);
	TRACE(Trace::little, optind);
	TRACE(Trace::little, key);                // to backend
	TRACE(Trace::little, waitForCompletion);
	TRACE(Trace::little, preMigrate);         // to backend
	TRACE(Trace::little, requestNumber);      // to backend
	TRACE(Trace::little, collocationFactor);  // to backend
	TRACE(Trace::little, fileList);
	TRACE(Trace::little, directoryName);      // to backend

	try {
		connect();
	}
	catch (...) {
		MSG(LTFSDMC0026E);
		return;
	}

	/* =================================================================
	   SPECIFIC SETTINGS
	   ================================================================= */

	LTFSDmProtocol::LTFSDmMigRequest *migreq = commCommand.mutable_migrequest();

	migreq->set_key(key);
	migreq->set_reqnumber(requestNumber);
	migreq->set_pid(getpid());

	if ( preMigrate == true )
		migreq->set_state(LTFSDmProtocol::LTFSDmMigRequest::PREMIGRATED);
	else
		migreq->set_state(LTFSDmProtocol::LTFSDmMigRequest::MIGRATED);

	if (!directoryName.compare(""))
		migreq->set_directory(true);
	else
		migreq->set_directory(false);

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
		MSG(LTFSDMC0028E);
		throw(LTFSDMErr::LTFSDM_GENERAL_ERROR);
	}

	const LTFSDmProtocol::LTFSDmMigRequestResp migreqresp = commCommand.migrequestresp();

	if( migreqresp.success() == true ) {
		if ( getpid() != migreqresp.pid() ) {
			MSG(LTFSDMC0036E);
			TRACE(Trace::error, getpid());
			TRACE(Trace::error, migreqresp.pid());
			throw(LTFSDMErr::LTFSDM_GENERAL_ERROR);
		}
		if ( requestNumber !=  migreqresp.reqnumber() ) {
			MSG(LTFSDMC0037E);
			TRACE(Trace::error, requestNumber);
			TRACE(Trace::error, migreqresp.reqnumber());
			throw(LTFSDMErr::LTFSDM_GENERAL_ERROR);
		}
	}
	else {
		MSG(LTFSDMC0029E);
		throw(LTFSDMErr::LTFSDM_GENERAL_ERROR);
	}

	/* =================================================================
	   SEND OBJECT INFORMATION
	   ================================================================= */


	std::ifstream filelist("XXX");
	std::string line;

	bool cont = true;

	int i;

	while (cont) {
		LTFSDmProtocol::LTFSDmSendObjects *sendobjects = commCommand.mutable_sendobjects();
		LTFSDmProtocol::LTFSDmSendObjects::FileName* filenames;
		for ( i = 0; (i < 7) && ((std::getline(filelist, line))); i++ ) {
			filenames = sendobjects->add_filenames();
			filenames->set_filename(line);
			printf("add: %s\n", line.c_str());
		}

		if ( i < 7 ) {
			cont = false;
			filenames = sendobjects->add_filenames();
			filenames->set_filename(""); //end
			printf("add: END\n");
		}

		try {
			commCommand.send();
		}
		catch(...) {
			printf("send error\n");
			exit(-1);
		}

		try {
			commCommand.recv();
		}
		catch(...) {
			printf("receive error\n");
			exit(-1);
		}

		if ( ! commCommand.has_migrequestresp() ) {
			std::cout << "wrong message sent from the server." << std::endl;
			return;
		}

		const LTFSDmProtocol::LTFSDmMigRequestResp migreqresp = commCommand.migrequestresp();

		if( migreqresp.success() == true ) {
			printf("sending success, reqNumber: %llu\n", (unsigned long long) migreqresp.reqnumber());
			if ( getpid() != migreqresp.pid() )
				printf("WRONG RESPONSE -- WRONG RESPONSE -- WRONG RESPONSE -- WRONG RESPONSE\n");
		}
		else	 {
			printf("error sending request\n");
		}
	} // end con

	filelist.close();


}
