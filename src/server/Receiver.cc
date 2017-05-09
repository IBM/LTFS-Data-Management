#include "ServerIncludes.h"

std::atomic<long> globalReqNumber;

void Receiver::run(long key, Connector *connector)

{
	MessageParser mproc;
	std::unique_lock<std::mutex> lock(Server::termmtx);
	SubServer subs(Const::MAX_RECEIVER_THREADS);
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
			subs.enqueue("MessageParser", &MessageParser::run, &mproc, key, command, connector);
		}
		catch(...) {
			MSG(LTFSDMS0010E);
			continue;
		}

		Server::termcond.wait_for(lock, std::chrono::seconds(30));
	}

	connector->terminate();

	command.closeRef();

	subs.waitAllRemaining();
}
