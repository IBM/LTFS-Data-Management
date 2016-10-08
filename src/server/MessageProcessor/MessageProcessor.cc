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

void MessageProcessor::getObjects(LTFSDmCommServer *command, long localReqNumber, unsigned long pid, long requestNumber)

{
	bool cont = true;

	while (cont) {
		try {
			command->recv();
		}
		catch(...) {
			TRACE(Trace::error, errno);
			MSG(LTFSDMS0006E);
			return;
		}

		if ( ! command->has_sendobjects() ) {
			TRACE(Trace::error, command->has_sendobjects());
			MSG(LTFSDMS0011E);
			return;
		}

		const LTFSDmProtocol::LTFSDmSendObjects sendobjects = command->sendobjects();

		for (int j = 0; j < sendobjects.filenames_size(); j++) {
			const LTFSDmProtocol::LTFSDmSendObjects::FileName& filename = sendobjects.filenames(j);
			if ( filename.filename().compare("") != 0 ) {
				std::cout << "file name:" << filename.filename().c_str() << std::endl;
			}
			else {
				cont = false;
				std::cout << "END" << std::endl;
			}
		}

		LTFSDmProtocol::LTFSDmSendObjectsResp *sendobjresp = command->mutable_sendobjectsresp();

		sendobjresp->set_success(true);
		sendobjresp->set_reqnumber(requestNumber);
		sendobjresp->set_pid(pid);

		try {
			command->send();
		}
		catch(...) {
			TRACE(Trace::error, errno);
			MSG(LTFSDMS0007E);
			return;
		}

		std::cout << "==============================" << std::endl;
	}
}

void MessageProcessor::migrationMessage(long key, LTFSDmCommServer *command, long localReqNumber)

{
	unsigned long pid;
	long requestNumber;

	std::cout << "Migration Request" << std::endl;
	const LTFSDmProtocol::LTFSDmMigRequest migreq = command->migrequest();
	std::cout << "key: " << migreq.key() << std::endl;
	requestNumber = migreq.reqnumber();
	std::cout << "reqNumber: " << requestNumber << std::endl;
	pid = migreq.pid();
	std::cout << "client pid: " <<  pid << std::endl;
	switch (migreq.state()) {
		case LTFSDmProtocol::LTFSDmMigRequest::MIGRATED:
			std::cout << "files to be migrated" << std::endl;
			break;
		case LTFSDmProtocol::LTFSDmMigRequest::PREMIGRATED:
			std::cout << "files to be premigrated" << std::endl;
			break;
		default:
			std::cout << "unkown target state" << std::endl;
	}

	LTFSDmProtocol::LTFSDmMigRequestResp *migreqresp = command->mutable_migrequestresp();

	migreqresp->set_success(true);
	migreqresp->set_reqnumber(requestNumber);
	migreqresp->set_pid(pid);

	try {
		command->send();
	}
	catch(...) {
		TRACE(Trace::error, errno);
		MSG(LTFSDMS0007E);
		return;
	}

	getObjects(command, localReqNumber, pid, requestNumber);
}

void  MessageProcessor::selRecallMessage(long key, LTFSDmCommServer *command, long localReqNumber)

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

void MessageProcessor::requestNumber(long key, LTFSDmCommServer *command, long *localReqNumber)

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
	*localReqNumber = ++globalReqNumber;
	reqnumresp->set_reqnumber(*localReqNumber);

	TRACE(Trace::little, *localReqNumber);

	try {
		command->send();
	}
	catch(...) {
		MSG(LTFSDMS0007E);
	}


}

void MessageProcessor::stop(long key, LTFSDmCommServer *command, long localReqNumber)

{
   	const LTFSDmProtocol::LTFSDmStopRequest stopreq = command->stoprequest();
	long keySent = stopreq.key();

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
	command->closeRef();
}

void MessageProcessor::status(long key, LTFSDmCommServer *command, long localReqNumber)

{
   	const LTFSDmProtocol::LTFSDmStatusRequest statusreq = command->statusrequest();
	long keySent = statusreq.key();

	TRACE(Trace::little, keySent);

	if ( key != keySent ) {
		MSG(LTFSDMS0008E);
		return;
	}

	LTFSDmProtocol::LTFSDmStatusResp *statusresp = command->mutable_statusresp();

	statusresp->set_success(true);

	//for testing

    // int duration = random() % 20;
    // sleep(duration);
	// statusresp->set_pid(duration);

	// if ( localReqNumber % 10 == 0 )
	// sleep(20);
	// statusresp->set_pid(localReqNumber);

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
	std::unique_lock<std::mutex> lock(termmtx);
	bool firstTime = true;
	long localReqNumber = Const::UNSET;

	while (true) {
		try {
			command.recv();
		}
		catch(...) {
			TRACE(Trace::error, errno);
			MSG(LTFSDMS0006E);
			lock.unlock();
			termcond.notify_one();
			return;
		}

		TRACE(Trace::much, "new message received");

		if ( command.has_reqnum() ) {
			requestNumber(key, &command, &localReqNumber);
		}
		else if ( command.has_stoprequest() ) {
			stop(key, &command, localReqNumber);
			terminate = true;
			lock.unlock();
			termcond.notify_one();
			break;
		}
		else {
			if ( firstTime ) {
				lock.unlock();
				termcond.notify_one();
				firstTime = false;
			}
			if ( command.has_migrequest() ) {
				migrationMessage(key, &command, localReqNumber);
			}
			else if ( command.has_selrecrequest() ) {
				selRecallMessage(key, &command, localReqNumber);
			}
			else if ( command.has_statusrequest() ) {
				status(key, &command, localReqNumber);
			}
			else {
				TRACE(Trace::error, "unkown command\n");
			}
			break;
		}
	}
	command.closeAcc();
}
