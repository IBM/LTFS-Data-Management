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

#include "src/server/SubServer.h"
#include "src/server/Server.h"
#include "src/server/Receiver.h"
#include "MessageProcessor.h"

void MessageProcessor::getObjects(LTFSDmCommServer *command, long localReqNumber, unsigned long pid, long requestNumber)

{
	bool cont = true;

	TRACE(Trace::much, __PRETTY_FUNCTION__);

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

	TRACE(Trace::much, __PRETTY_FUNCTION__);

	std::cout << "Migration Request" << std::endl;
	const LTFSDmProtocol::LTFSDmMigRequest migreq = command->migrequest();
	std::cout << "key: " << migreq.key() << std::endl;
	requestNumber = migreq.reqnumber();
	std::cout << "reqNumber: " << requestNumber << std::endl;
	pid = migreq.pid();
	std::cout << "client pid: " <<  pid << std::endl;
	std::cout << "colocation factor: " << migreq.colfactor() << std::endl;
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
	unsigned long pid;
	long requestNumber;

	TRACE(Trace::much, __PRETTY_FUNCTION__);

	std::cout << "Recall Request" << std::endl;
	const LTFSDmProtocol::LTFSDmSelRecRequest recreq = command->selrecrequest();
	std::cout << "key: " << recreq.key() << std::endl;
	requestNumber = recreq.reqnumber();
	std::cout << "reqNumber: " << requestNumber << std::endl;
	pid = recreq.pid();
	std::cout << "client pid: " <<  pid << std::endl;
	switch (recreq.state()) {
		case LTFSDmProtocol::LTFSDmSelRecRequest::RESIDENT:
			std::cout << "files to be recalled to resident state" << std::endl;
			break;
		case LTFSDmProtocol::LTFSDmSelRecRequest::PREMIGRATED:
			std::cout << "files to be recalled to premigrated state" << std::endl;
			break;
		default:
			std::cout << "unkown target state" << std::endl;
	}

	LTFSDmProtocol::LTFSDmSelRecRequestResp *recreqresp = command->mutable_selrecrequestresp();

	recreqresp->set_success(true);
	recreqresp->set_reqnumber(requestNumber);
	recreqresp->set_pid(pid);

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

void MessageProcessor::infoFilesMessage(long key, LTFSDmCommServer *command, long localReqNumber)

{
	unsigned long pid;
	long requestNumber;

	TRACE(Trace::much, __PRETTY_FUNCTION__);

	std::cout << "Info Files Request" << std::endl;
	const LTFSDmProtocol::LTFSDmInfoFilesRequest infofilesreq = command->infofilesrequest();
	std::cout << "key: " << infofilesreq.key() << std::endl;
	requestNumber = infofilesreq.reqnumber();
	std::cout << "reqNumber: " << requestNumber << std::endl;
	pid = infofilesreq.pid();
	std::cout << "client pid: " <<  pid << std::endl;

	LTFSDmProtocol::LTFSDmInfoFilesRequestResp *infofilesreqresp = command->mutable_infofilesrequestresp();

	infofilesreqresp->set_success(true);
	infofilesreqresp->set_reqnumber(requestNumber);
	infofilesreqresp->set_pid(pid);

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

void MessageProcessor::requestNumber(long key, LTFSDmCommServer *command, long *localReqNumber)

{
   	const LTFSDmProtocol::LTFSDmReqNumber reqnum = command->reqnum();
	long keySent = reqnum.key();

	TRACE(Trace::much, __PRETTY_FUNCTION__);
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

void MessageProcessor::stopMessage(long key, LTFSDmCommServer *command, long localReqNumber)

{
   	const LTFSDmProtocol::LTFSDmStopRequest stopreq = command->stoprequest();
	long keySent = stopreq.key();

	TRACE(Trace::little, keySent);
	TRACE(Trace::much, __PRETTY_FUNCTION__);

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

void MessageProcessor::statusMessage(long key, LTFSDmCommServer *command, long localReqNumber)

{
   	const LTFSDmProtocol::LTFSDmStatusRequest statusreq = command->statusrequest();
	long keySent = statusreq.key();

	TRACE(Trace::little, keySent);
	TRACE(Trace::much, __PRETTY_FUNCTION__);

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
			stopMessage(key, &command, localReqNumber);
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
			else if ( command.has_infofilesrequest() ) {
				infoFilesMessage(key, &command, localReqNumber);
			}
			else if ( command.has_statusrequest() ) {
				statusMessage(key, &command, localReqNumber);
			}
			else {
				TRACE(Trace::error, "unkown command\n");
			}
			break;
		}
	}
	command.closeAcc();
}
