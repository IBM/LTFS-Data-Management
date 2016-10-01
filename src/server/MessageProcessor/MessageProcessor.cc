#include <string>
#include <atomic>
#include <condition_variable>
#include <thread>

#include "src/common/messages/Message.h"
#include "src/common/tracing/Trace.h"
#include "src/common/errors/errors.h"
#include "src/common/const/Const.h"

#include "src/common/comm/ltfsdm.pb.h"
#include "src/common/comm/LTFSDmComm.h"

#include "src/server/SubServer/SubServer.h"
#include "src/server/Server.h"
#include "src/server/Receiver/Receiver.h"
#include "MessageProcessor.h"

void MessageProcessor::migrationMessage(long key, LTFSDmCommServer *command)

{
   	const LTFSDmProtocol::LTFSDmMigRequest migreq = command->migrequest();
	unsigned long pid;
	long keySent = 	migreq.key();

	TRACE(Trace::little, keySent);

	if ( key != keySent ) {
		MSG(LTFSDMS0008E);
		return;
	}

	TRACE(Trace::little, migreq.reqnumber());
	pid = migreq.pid();
	TRACE(Trace::little, pid);
	switch (migreq.state()) {
		case LTFSDmProtocol::LTFSDmMigRequest::MIGRATED:
			TRACE(Trace::little, "files to be migrated\n");
			break;
		case LTFSDmProtocol::LTFSDmMigRequest::PREMIGRATED:
			TRACE(Trace::little, "files to be premigrated\n");
			break;
		default:
			TRACE(Trace::little, "unkown target state\n");
	}

	for (int j = 0; j < migreq.filenames_size(); j++) {
		const LTFSDmProtocol::LTFSDmMigRequest::FileName& filename = migreq.filenames(j);
		TRACE(Trace::little, filename.filename().c_str());
	}

	// RESPONSE

	LTFSDmProtocol::LTFSDmMigRequestResp *migreqresp = command->mutable_migrequestresp();

	migreqresp->set_success(true);
	migreqresp->set_reqnumber(time(NULL));
	migreqresp->set_pid(pid);

	try {
		command->send();
	}
	catch(...) {
		MSG(LTFSDMS0007E);
		throw(LTFSDMErr::LTFSDM_GENERAL_ERROR);
	}
}

void  MessageProcessor::selRecallMessage(long key, LTFSDmCommServer *command)

{
	const LTFSDmProtocol::LTFSDmSelRecRequest selrecreq = command->selrecrequest();
	long keySent = 	selrecreq.key();

	TRACE(Trace::little, keySent);

	if ( key != keySent ) {
		MSG(LTFSDMS0008E);
		return;
	}

	TRACE(Trace::little, selrecreq.reqnumber());
	switch (selrecreq.state()) {
		case LTFSDmProtocol::LTFSDmSelRecRequest::MIGRATED:
			TRACE(Trace::little, "files to be migrated\n");
			break;
		case LTFSDmProtocol::LTFSDmSelRecRequest::PREMIGRATED:
			TRACE(Trace::little, "files to be premigrated\n");
			break;
		default:
			TRACE(Trace::little, "unkown target state\n");
	}

	for (int j = 0; j < selrecreq.filenames_size(); j++) {
		const LTFSDmProtocol::LTFSDmSelRecRequest::FileName& filename = selrecreq.filenames(j);
		TRACE(Trace::little, filename.filename().c_str());
	}
}

void MessageProcessor::requestNumber(long key, LTFSDmCommServer *command)

{
   	const LTFSDmProtocol::LTFSDmReqNumber reqnum = command->reqnum();
	long keySent = reqnum.key();

	TRACE(Trace::little, keySent);

	if ( key != keySent ) {
		MSG(LTFSDMS0008E);
		return;
	}

	LTFSDmProtocol::LTFSDmReqNumberResp *reqnumresp = command->mutable_reqnumresp();

	reqnumresp->set_success(true);
	reqNumber++;
	reqnumresp->set_reqnumber(reqNumber);

	TRACE(Trace::little, (long) reqNumber);

	try {
		command->send();
	}
	catch(...) {
		MSG(LTFSDMS0007E);
	}


}

void MessageProcessor::stop(long key, LTFSDmCommServer *command)

{
   	const LTFSDmProtocol::LTFSDmStopRequest stopreq = command->stoprequest();
	long keySent = stopreq.key();

	cont = false;

	TRACE(Trace::little, keySent);

	if ( key != keySent ) {
		MSG(LTFSDMS0008E);
		return;
	}

	MSG(LTFSDMS0009I);

	LTFSDmProtocol::LTFSDmStopResp *stopresp = command->mutable_stopresp();

	stopresp->set_success(true);

	try {
		command->send();
	}
	catch(...) {
		MSG(LTFSDMS0007E);
	}

	terminate = true;
	command->closeRef();
}

void MessageProcessor::status(long key, LTFSDmCommServer *command)

{
   	const LTFSDmProtocol::LTFSDmStatusRequest statusreq = command->statusrequest();
	long keySent = statusreq.key();

	cont = false;

	TRACE(Trace::little, keySent);

	if ( key != keySent ) {
		MSG(LTFSDMS0008E);
		return;
	}

	LTFSDmProtocol::LTFSDmStatusResp *statusresp = command->mutable_statusresp();

	statusresp->set_success(true);
	statusresp->set_pid(getpid());

	try {
		command->send();
	}
	catch(...) {
		MSG(LTFSDMS0007E);
	}
}

void MessageProcessor::run(std::string label, long key, LTFSDmCommServer command)

{
	TRACE(Trace::much, __PRETTY_FUNCTION__);

	while (cont == true &&  terminate == false) {
		try {
			command.recv();
		}
		catch(...) {
			MSG(LTFSDMS0006E);
			break;
		}

		TRACE(Trace::much, "new message received");

		// MIGRATION
		if ( command.has_migrequest() ) {
			migrationMessage(key, &command);
		}
		// SELECTIVE RECALL
		else if ( command.has_selrecrequest() ) {
			selRecallMessage(key, &command);
		}
		else if ( command.has_reqnum() ) {
			requestNumber(key, &command);
		}
		else if ( command.has_stoprequest() ) {
			stop(key, &command);
		}
		else if ( command.has_statusrequest() ) {
			status(key, &command);
		}
		else {
			TRACE(Trace::error, "unkown command\n");
		}
	}
	command.closeAcc();
}
