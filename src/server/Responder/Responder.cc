#include <string>
#include <thread>

#include "src/common/tracing/Trace.h"

#include "Responder.h"

void Responder::run(std::string label, long key)

{
	while (true) {
		sleep(1);
		TRACE(Trace::error, label);
	}
}
