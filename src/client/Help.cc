#include <string>
#include "src/common/messages/messages.h"
#include "Operation.h"
#include "Help.h"

Help::Help()

{
}

void Help::printUsage()
{
	MSG_INFO(OLTFSC0008I);
}

void Help::doOperation(int argc, char **argv)
{
	printUsage();
}
