#include <string>

#include "src/common/messages/messages.h"
#include "src/common/tracing/trace.h"
#include "src/common/errors/errors.h"

#include "OpenLTFSCommand.h"
#include "StartCommand.h"
#include "StopCommand.h"
#include "MigrationCommand.h"
#include "RecallCommand.h"
#include "InfoCommand.h"
#include "InfoRequestsCommand.h"
#include "InfoFilesCommand.h"
#include "HelpCommand.h"

void HelpCommand::printUsage()
{
	MSG_INFO(OLTFSC0008I);
	MSG_INFO(OLTFSC0020I);
}

void HelpCommand::doCommand(int argc, char **argv)

{
	OpenLTFSCommand *openLTFSCommand = NULL;
	std::string command;

	if ( argc == 1 ) {
		printUsage();
		return;
	}

	TRACE(0, argc);
	TRACE(0, command.c_str());

	command = std::string(argv[1]);

 	if  ( !command.compare(StartCommand::command) ) {
		openLTFSCommand = new StartCommand();
	}
	else if ( !command.compare(StopCommand::command) ) {
		openLTFSCommand = new StopCommand();
	}
	else if ( !command.compare(MigrationCommand::command) ) {
		openLTFSCommand = new MigrationCommand();
	}
	else if ( !command.compare(RecallCommand::command) ) {
		openLTFSCommand = new RecallCommand();
	}
	else if ( !command.compare(HelpCommand::command) ) {
		openLTFSCommand = new HelpCommand();
	}
	else if ( !command.compare(InfoCommand::command) ) {
		if ( argc < 3 ) {
			MSG_INFO(OLTFSC0020I);
			return;
		}
		command = std::string(argv[2]);
		TRACE(0, command.c_str());
		if      ( !command.compare(InfoRequestsCommand::command) ) {
			openLTFSCommand = new InfoRequestsCommand();
		}
		else if ( !command.compare(InfoFilesCommand::command) ) {
			openLTFSCommand = new InfoFilesCommand();
		}
		else {
			MSG_INFO(OLTFSC0020I);
			return;
		}
	}
	else {
		printUsage();
		return;
	}

	openLTFSCommand->printUsage();
}
