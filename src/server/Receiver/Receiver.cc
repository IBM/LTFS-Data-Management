#include <string>
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
#include "src/server/MessageProcessor/MessageProcessor.h"

#include "Receiver.h"

std::atomic<long> globalReqNumber;

void Receiver::run(std::string label, long key)

{
	MessageProcessor mproc;
	std::unique_lock<std::mutex> lock(mproc.termmtx);
	LTFSDmCommServer command;
	SubServer subs(40);

	TRACE(Trace::much, __PRETTY_FUNCTION__);

	globalReqNumber = 0;

	try {
		command.listen();
	}
	catch(...) {
		MSG(LTFSDMS0004E);
		throw(LTFSDMErr::LTFSDM_GENERAL_ERROR);
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
			subs.enqueue(&MessageProcessor::run, &mproc, "MessageProcessor", key, command);
		}
		catch(...) {
			MSG(LTFSDMS0010E);
			continue;
		}

		mproc.termcond.wait_for(lock, std::chrono::seconds(30));
	}

	command.closeRef();

	subs.waitAllRemaining();
}
