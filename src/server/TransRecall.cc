#include <limits.h>

#include <atomic>
#include <condition_variable>
#include <mutex>
#include <thread>

#include "src/common/util/util.h"
#include "src/common/messages/Message.h"
#include "src/common/tracing/Trace.h"
#include "src/common/errors/errors.h"
#include "src/common/const/Const.h"

#include "src/connector/Connector.h"
#include "SubServer.h"
#include "Server.h"
#include "TransRecall.h"


void TransRecall::run(Connector connector)

{
/*
	Connector::rec_info_t recinfo;

	try {
		connector.initTransRecalls();
	}
	catch ( int error ) {
		MSG(LTFSDMS0030E);
		return;
	}

	while (terminate == false) {
		recinfo = connector.getEvents();
	}
*/
}
