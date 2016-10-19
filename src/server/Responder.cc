#include "limits.h"

#include <string>
#include <condition_variable>
#include <thread>

#include "src/common/tracing/Trace.h"

#include "src/connector/Connector.h"
#include "src/server/SubServer.h"
#include "src/server/Server.h"
#include "Responder.h"

void Responder::run(std::string label, long key)

{
	TRACE(Trace::much, __PRETTY_FUNCTION__);

	while (terminate == false) {
		sleep(1);
		TRACE(Trace::error, label);
	}
}
