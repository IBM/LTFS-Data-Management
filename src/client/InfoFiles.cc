#include <string>
#include "src/common/messages/messages.h"
#include "Operation.h"
#include "InfoFiles.h"

InfoFiles::InfoFiles()

{
}

void InfoFiles::printUsage()
{
	MSG_INFO(OLTFSC0010I);
}

void InfoFiles::doOperation(int argc, char **argv)
{
	printUsage();
}
