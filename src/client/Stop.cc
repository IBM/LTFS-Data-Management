#include <string>
#include "src/common/messages/messages.h"
#include "Operation.h"
#include "Stop.h"

Stop::Stop()

{
}

void Stop::printUsage()
{
	MSG_INFO(OLTFSC0007I);
}

void Stop::doOperation(int argc, char **argv)
{
	printUsage();
}
