#include <string>
#include <thread>

#include "src/common/tracing/Trace.h"

#include "src/server/ServerComponent/ServerComponent.h"
#include "Receiver.h"

void Receiver::run(std::string info)

{
	while (true) {
		sleep(1);
		TRACE(Trace::error, info);
		MSG(LTFSDMS0003X);
	}
}
