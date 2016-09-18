#include <unistd.h>
#include <signal.h>
#include <string>

#include "src/common/messages/messages.h"
#include "src/common/tracing/trace.h"
#include "src/common/errors/errors.h"


int main(int argc, char **argv)

{
	TRACE(0, "Server started");
	TRACE(0, getpid());
	sleep(5);

	return 0;
}
