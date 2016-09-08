#include <string>
#include "src/common/messages/messages.h"
#include "Operation.h"
#include "List.h"

List::List()

{
}

void List::printUsage()
{
	MSG_INFO(OLTFSC0003I);
}

void List::doOperation(int argc, char **argv)
{
	printUsage();
}
