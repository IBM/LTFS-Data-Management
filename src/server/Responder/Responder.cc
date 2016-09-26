#include <string>
#include <thread>

#include "src/common/tracing/Trace.h"

#include "src/server/ServerComponent/ServerComponent.h"
#include "Responder.h"

void Responder::run(long key)

{
	while (true) {
		sleep(1);
		TRACE(Trace::error, key);
	}
}
