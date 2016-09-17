#include <string>
#include "src/common/messages/messages.h"
#include "src/common/tracing/trace.h"
#include "src/common/errors/errors.h"

#include "OpenLTFSCommand.h"
#include "StartCommand.h"
#include "StopCommand.h"
#include "MigrationCommand.h"
#include "RecallCommand.h"
#include "HelpCommand.h"
#include "InfoCommand.h"
#include "InfoRequestsCommand.h"
#include "InfoFilesCommand.h"

int main(int argc, char *argv[])

{
	OpenLTFSCommand *operation = NULL;
	std::string command;

	if ( argc < 2 ) {
		operation = new HelpCommand();
		operation->doCommand(argc, argv);
		return (int) OLTFSErr::OLTFS_GENERAL_ERROR;
	}

	command = std::string(argv[1]);

	TRACE(0, argc);
	TRACE(0, command.c_str());

 	if  ( !command.compare(StartCommand::command) ) {
		operation = new StartCommand();
	}
	else if ( !command.compare(StopCommand::command) ) {
		operation = new StopCommand();
	}
	else if ( !command.compare(MigrationCommand::command) ) {
		operation = new MigrationCommand();
	}
	else if ( !command.compare(RecallCommand::command) ) {
		operation = new RecallCommand();
	}
	else if ( !command.compare(HelpCommand::command) ) {
		operation = new HelpCommand();
	}
	else if ( !command.compare(InfoCommand::command) ) {
		if ( argc < 3 ) {
			MSG_OUT(OLTFSC0011E);
			return (int) OLTFSErr::OLTFS_GENERAL_ERROR;
		}
		argc--;
		argv++;
		command = std::string(argv[1]);
		TRACE(0, command.c_str());
		if      ( !command.compare(InfoRequestsCommand::command) ) {
			operation = new InfoRequestsCommand();
		}
		else if ( !command.compare(InfoFilesCommand::command) ) {
			operation = new InfoFilesCommand();
		}
		else {
			MSG_OUT(OLTFSC0012E, command.c_str());
			operation = new HelpCommand();
			return (int) OLTFSErr::OLTFS_GENERAL_ERROR;
		}
	}
	else {
		MSG_OUT(OLTFSC0005E, command.c_str());
		operation = new HelpCommand();
		return (int) OLTFSErr::OLTFS_GENERAL_ERROR;
	}

	TRACE(0, operation);

	argc--;
	argv++;

	if (argc > 1) {
		TRACE(0, argc);
		TRACE(0, argv[1]);
	}

	try {
		operation->doCommand(argc, argv);
	}
	catch(OLTFSErr err) {
		return (int) err;
	}

	return (int) OLTFSErr::OLTFS_OK;
}
