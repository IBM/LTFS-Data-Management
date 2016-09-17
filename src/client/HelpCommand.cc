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

 	if  ( StartCommand().compare(command) ) {
		openLTFSCommand = new StartCommand();
	}
	else if ( StopCommand().compare(command) ) {
		openLTFSCommand = new StopCommand();
	}
	else if ( MigrationCommand().compare(command) ) {
		openLTFSCommand = new MigrationCommand();
	}
	else if ( RecallCommand().compare(command) ) {
		openLTFSCommand = new RecallCommand();
	}
	else if ( HelpCommand().compare(command) ) {
		openLTFSCommand = new HelpCommand();
	}
	else if ( InfoCommand().compare(command) ) {
		if ( argc < 3 ) {
			openLTFSCommand = new InfoCommand();
		}
		else {
			command = std::string(argv[2]);
			TRACE(0, command.c_str());
			if      ( InfoRequestsCommand().compare(command) ) {
				openLTFSCommand = new InfoRequestsCommand();
			}
			else if ( InfoFilesCommand().compare(command) ) {
				openLTFSCommand = new InfoFilesCommand();
			}
			else {
				openLTFSCommand = new InfoCommand();
			}
		}
	}
	else {
		printUsage();
		goto cleanup;
	}

	openLTFSCommand->printUsage();
cleanup:
	if (openLTFSCommand) delete(openLTFSCommand);
}
