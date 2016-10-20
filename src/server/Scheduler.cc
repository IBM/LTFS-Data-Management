#include "limits.h"

#include <string>
#include <condition_variable>
#include <thread>
#include <typeinfo>

#include "src/common/tracing/Trace.h"

#include "src/common/util/util.h"
#include "src/connector/Connector.h"
#include "src/server/SubServer.h"
#include "src/server/Server.h"
#include "Scheduler.h"

void Scheduler::run(long key)

{
	TRACE(Trace::much, __PRETTY_FUNCTION__);

	while (terminate == false) {
		sleep(1);
	}
}
