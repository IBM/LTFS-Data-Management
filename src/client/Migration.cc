#include <string>
#include "src/common/messages/messages.h"
#include "Operation.h"
#include "Migration.h"

Migration::Migration()

{
}

void Migration::printUsage()
{
	MSG_INFO(OLTFSC0001I);
}

void Migration::doOperation(int argc, char **argv)
{
	printUsage();
}
