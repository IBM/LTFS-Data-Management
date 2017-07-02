#include "ServerIncludes.h"

std::atomic<long> globalReqNumber;

void Receiver::run(long key, Connector *connector)

{
	MessageParser mproc;
	std::unique_lock<std::mutex> lock(Server::termmtx);
	WorkQueue<long, LTFSDmCommServer, Connector*> wq(&MessageParser::run, Const::MAX_RECEIVER_THREADS, "msg-wq");
	LTFSDmCommServer command;

	TRACE(Trace::much, __PRETTY_FUNCTION__);

	globalReqNumber = 0;

	try {
		command.listen();
	}
	catch(...) {
		MSG(LTFSDMS0004E);
		throw(Error::LTFSDM_GENERAL_ERROR);
	}

	while (terminate == false) {
		try {
			command.accept();
		}
		catch(...) {
			MSG(LTFSDMS0005E);
			break;
		}

		try {
			wq.enqueue(Const::UNSET, key, command, connector);
		}
		catch(...) {
			MSG(LTFSDMS0010E);
			continue;
		}

		Server::termcond.wait_for(lock, std::chrono::seconds(30));
	}

	connector->terminate();

	command.closeRef();

	wq.waitCompletion(Const::UNSET);
	wq.terminate();
}
