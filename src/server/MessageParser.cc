#include <string>
#include <atomic>
#include <condition_variable>
#include <thread>

#include <sqlite3.h>

#include "src/common/messages/Message.h"
#include "src/common/tracing/Trace.h"
#include "src/common/errors/errors.h"
#include "src/common/const/Const.h"

#include "src/common/comm/ltfsdm.pb.h"
#include "src/common/comm/LTFSDmComm.h"

#include "src/connector/Connector.h"
#include "src/server/SubServer.h"
#include "src/server/Server.h"
#include "src/server/Receiver.h"

#include "FileOperation.h"
#include "Migration.h"
#include "SelRecall.h"
#include "InfoFiles.h"

#include "MessageParser.h"

void MessageParser::getObjects(LTFSDmCommServer *command, long localReqNumber, unsigned long pid, long requestNumber, FileOperation *fopt)

{
	bool cont = true;
	bool success = true;

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
					try {
						fopt->addFileName(filename.filename());
					}
					catch(int error) {
						if ( error == SQLITE_CONSTRAINT_PRIMARYKEY ||
							 error == SQLITE_CONSTRAINT_UNIQUE)
							MSG(LTFSDMS0019E, filename.filename().c_str());
						else
							MSG(LTFSDMS0015E, filename.filename().c_str(), sqlite3_errstr(error));
					}
				}
				else {
					cont = false; // END
				}
			}

		LTFSDmProtocol::LTFSDmSendObjectsResp *sendobjresp = command->mutable_sendobjectsresp();

		sendobjresp->set_success(success);
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
	}
}

void MessageParser::migrationMessage(long key, LTFSDmCommServer *command, long localReqNumber)

{
	unsigned long pid;
	long requestNumber;
	const LTFSDmProtocol::LTFSDmMigRequest migreq = command->migrequest();
	long keySent = migreq.key();

	TRACE(Trace::much, __PRETTY_FUNCTION__);
	TRACE(Trace::little, keySent);

	if ( key != keySent ) {
		MSG(LTFSDMS0008E);
		return;
	}

	requestNumber = migreq.reqnumber();
	pid = migreq.pid();

	Migration mig( pid, requestNumber, migreq.colfactor(), migreq.state());

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

	getObjects(command, localReqNumber, pid, requestNumber, dynamic_cast<FileOperation*> (&mig));

	mig.addRequest();
}

void  MessageParser::selRecallMessage(long key, LTFSDmCommServer *command, long localReqNumber)

{
	unsigned long pid;
	long requestNumber;
	const LTFSDmProtocol::LTFSDmSelRecRequest recreq = command->selrecrequest();
	long keySent = recreq.key();

	TRACE(Trace::much, __PRETTY_FUNCTION__);
	TRACE(Trace::little, keySent);

	if ( key != keySent ) {
		MSG(LTFSDMS0008E);
		return;
	}

	requestNumber = recreq.reqnumber();
	pid = recreq.pid();

	SelRecall srec( pid, requestNumber, recreq.state());

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

	getObjects(command, localReqNumber, pid, requestNumber, dynamic_cast<FileOperation*> (&srec));

	srec.addRequest();
}

void MessageParser::infoFilesMessage(long key, LTFSDmCommServer *command, long localReqNumber)

{
	unsigned long pid;
	long requestNumber;
	const LTFSDmProtocol::LTFSDmInfoFilesRequest infofilesreq = command->infofilesrequest();
	long keySent = infofilesreq.key();

	TRACE(Trace::much, __PRETTY_FUNCTION__);
	TRACE(Trace::little, keySent);

	if ( key != keySent ) {
		MSG(LTFSDMS0008E);
		return;
	}

	requestNumber = infofilesreq.reqnumber();
	pid = infofilesreq.pid();

	InfoFiles infofiles( pid, requestNumber);

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

	getObjects(command, localReqNumber, pid, requestNumber, dynamic_cast<FileOperation*> (&infofiles));

	infofiles.start();
}

void MessageParser::requestNumber(long key, LTFSDmCommServer *command, long *localReqNumber)

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

void MessageParser::stopMessage(long key, LTFSDmCommServer *command, long localReqNumber)

{
   	const LTFSDmProtocol::LTFSDmStopRequest stopreq = command->stoprequest();
	long keySent = stopreq.key();

	TRACE(Trace::much, __PRETTY_FUNCTION__);
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

void MessageParser::statusMessage(long key, LTFSDmCommServer *command, long localReqNumber)

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

void MessageParser::run(long key, LTFSDmCommServer command)

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