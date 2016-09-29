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
#include "src/server/MessageProcessor/MessageProcessor.h"
#include "src/server/SubServer/SubServer.h"

#include "Receiver.h"

std::atomic<long> reqNumber;

void Receiver::run(std::string label, long key)

{
	LTFSDmCommServer command;
	SubServer subs(40);

	reqNumber = 0;

	try {
		command.listen();
	}
	catch(...) {
		MSG(LTFSDMS0004E);
		throw(LTFSDMErr::LTFSDM_GENERAL_ERROR);
	}

	while (true) {
		try {
			command.accept();
		}
		catch(...) {
			MSG(LTFSDMS0005E);
			throw(LTFSDMErr::LTFSDM_GENERAL_ERROR);
		}

		try {
			command.recv();
		}
		catch(...) {
			MSG(LTFSDMS0006E);
			throw(LTFSDMErr::LTFSDM_GENERAL_ERROR);
		}

 		MessageProcessor *mproc = new MessageProcessor();
 		subs.enqueue(&MessageProcessor::run, mproc, "MessageProcessor", key, &command);
	}

	subs.waitAllRemaining();
}
