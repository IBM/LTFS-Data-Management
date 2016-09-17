#include <string>
#include "src/common/messages/messages.h"
#include "OpenLTFSCommand.h"
#include "HelpCommand.h"

void HelpCommand::printUsage()
{
	MSG_INFO(OLTFSC0003I);
}

void HelpCommand::doCommand(int argc, char **argv)
{
	MSG_INFO(OLTFSC0008I);
}
