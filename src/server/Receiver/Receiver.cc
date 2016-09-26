#include <string>
#include <thread>

#include "src/common/messages/Message.h"
#include "src/common/tracing/Trace.h"
#include "src/common/errors/errors.h"
#include "src/common/const/Const.h"

#include "src/common/comm/ltfsdm.pb.h"
#include "src/common/comm/LTFSDmComm.h"

#include "src/server/ServerComponent/ServerComponent.h"
#include "src/server/SubServer/SubServer.h"
#include "src/server/MessageProcessor/MessageProcessor.h"
#include "src/server/SubServer/SubServer.h"

#include "Receiver.h"

void Receiver::run(ReceiverData data)

{
	LTFSDmCommServer command;
	SubServer subs;

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

		// command.ParseFromFileDescriptor(cl);
		try {
			command.recv();
		}
		catch(...) {
			MSG(LTFSDMS0006E);
			throw(LTFSDMErr::LTFSDM_GENERAL_ERROR);
		}

		MessageProcessor *mproc = new MessageProcessor(MessageProcessorData("MessageProcessor", data.key, command));

		subs.add(mproc);

		sleep(1);
	}

	subs.wait();
	/*
	  while (true) {
	  sleep(1);
	  TRACE(Trace::error, data.label);
	  TRACE(Trace::error, data.key);
	  MSG(LTFSDMS0003X);
	  }
	*/
}
