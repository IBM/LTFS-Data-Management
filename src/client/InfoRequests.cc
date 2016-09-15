#include <string>
#include "src/common/messages/messages.h"
#include "Operation.h"
#include "InfoRequests.h"

InfoRequests::InfoRequests()

{
}

void InfoRequests::printUsage()
{
	MSG_INFO(OLTFSC0009I);
}

void InfoRequests::doOperation(int argc, char **argv)
{
	printUsage();
}
