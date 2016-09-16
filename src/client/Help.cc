#include <string>
#include "src/common/messages/messages.h"
#include "Operation.h"
#include "Help.h"

void Help::printUsage()
{
	MSG_INFO(OLTFSC0003I);
}

void Help::doOperation(int argc, char **argv)
{
	MSG_INFO(OLTFSC0008I);
}
