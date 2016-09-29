#include <string>
#include <thread>

#include "src/common/tracing/Trace.h"

#include "Responder.h"

void Responder::run(std::string label, long key)

{
	TRACE(Trace::much, __PRETTY_FUNCTION__);

	while (true) {
		sleep(1);
		TRACE(Trace::error, label);
	}
}
