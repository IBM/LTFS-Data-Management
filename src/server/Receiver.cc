#include <sys/resource.h>

#include <string>
#include <condition_variable>
#include <thread>

#include "src/common/messages/Message.h"
#include "src/common/tracing/Trace.h"
#include "src/common/errors/errors.h"
#include "src/common/const/Const.h"

#include "src/common/comm/ltfsdm.pb.h"
#include "src/common/comm/LTFSDmComm.h"

#include "src/connector/Connector.h"
#include "SubServer.h"
#include "Server.h"
#include "FileOperation.h"
#include "MessageParser.h"

#include "Receiver.h"

std::atomic<long> globalReqNumber;

void Receiver::run(long key, Connector *connector)

{
	MessageParser mproc;
	std::unique_lock<std::mutex> lock(mproc.termmtx);
	LTFSDmCommServer command;
	SubServer subs(Const::MAX_RECEIVER_THREADS);

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

		mproc.termcond.wait_for(lock, std::chrono::seconds(30));
	}

	connector->terminate();

	command.closeRef();

	subs.waitAllRemaining();
}
