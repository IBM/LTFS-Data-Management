#include <string>
#include "src/common/messages/messages.h"
#include "Operation.h"
#include "Start.h"

Start::Start()

{
}

void Start::printUsage()
{
	MSG_INFO(OLTFSC0006I);
}

void Start::doOperation(int argc, char **argv)
{
	printUsage();
}
