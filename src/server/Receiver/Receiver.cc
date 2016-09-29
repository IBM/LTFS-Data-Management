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

std::atomic<long> reqNumber;

void Receiver::run(std::string label, long key)

{
	LTFSDmCommServer command;
	SubServer subs(40);

	TRACE(Trace::much, __PRETTY_FUNCTION__);

	reqNumber = 0;

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
			//throw(LTFSDMErr::LTFSDM_GENERAL_ERROR);
			break;
		}

 		MessageProcessor *mproc = new MessageProcessor();
 		subs.enqueue(&MessageProcessor::run, mproc, "MessageProcessor", key, &command);
	}

	subs.waitAllRemaining();
}
