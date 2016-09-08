#include <string>
#include "src/common/messages/messages.h"
#include "Operation.h"
#include "Recall.h"

Recall::Recall()

{
}

void Recall::printUsage()
{
	MSG_INFO(OLTFSC0002I);
}

void Recall::doOperation(int argc, char **argv)
{
	printUsage();
}
