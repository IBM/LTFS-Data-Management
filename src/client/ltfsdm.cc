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
	OpenLTFSCommand *openLTFSCommand = NULL;
	std::string command;
	OLTFSErr rc = OLTFSErr::OLTFS_OK ;

	if ( argc < 2 ) {
		openLTFSCommand = new HelpCommand();
		openLTFSCommand->doCommand(argc, argv);
		rc = OLTFSErr::OLTFS_GENERAL_ERROR;
		goto cleanup;
	}

	command = std::string(argv[1]);

	TRACE(0, argc);
	TRACE(0, command.c_str());

 	if  ( !command.compare(StartCommand().getcmd()) ) {
		openLTFSCommand = new StartCommand();
	}
	else if ( !command.compare(StopCommand().getcmd()) ) {
		openLTFSCommand = new StopCommand();
	}
	else if ( !command.compare(MigrationCommand().getcmd()) ) {
		openLTFSCommand = new MigrationCommand();
	}
	else if ( !command.compare(RecallCommand().getcmd()) ) {
		openLTFSCommand = new RecallCommand();
	}
	else if ( !command.compare(HelpCommand().getcmd()) ) {
		openLTFSCommand = new HelpCommand();
	}
	else if ( !command.compare(InfoCommand().getcmd()) ) {
		if ( argc < 3 ) {
			MSG_OUT(OLTFSC0011E);
			rc =  OLTFSErr::OLTFS_GENERAL_ERROR;
			goto cleanup;
		}
		argc--;
		argv++;
		command = std::string(argv[1]);
		TRACE(0, command.c_str());
		if      ( !command.compare(InfoRequestsCommand().getcmd()) ) {
			openLTFSCommand = new InfoRequestsCommand();
		}
		else if ( !command.compare(InfoFilesCommand().getcmd()) ) {
			openLTFSCommand = new InfoFilesCommand();
		}
		else {
			MSG_OUT(OLTFSC0012E, command.c_str());
			openLTFSCommand = new HelpCommand();
			rc = OLTFSErr::OLTFS_GENERAL_ERROR;
			goto cleanup;
		}
	}
	else {
		MSG_OUT(OLTFSC0005E, command.c_str());
		openLTFSCommand = new HelpCommand();
		rc = OLTFSErr::OLTFS_GENERAL_ERROR;
		goto cleanup;
	}

	TRACE(0, openLTFSCommand);

	argc--;
	argv++;

	if (argc > 1) {
		TRACE(0, argc);
		TRACE(0, argv[1]);
	}

	try {
		openLTFSCommand->doCommand(argc, argv);
	}
	catch(OLTFSErr err) {
		rc = err;
		goto cleanup;
	}

cleanup:
	if (openLTFSCommand)
		delete(openLTFSCommand);

	return (int) rc;
}
