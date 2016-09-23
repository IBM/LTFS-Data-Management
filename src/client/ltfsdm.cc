#include <string>
#include <sstream>

#include "src/common/util/util.h"
#include "src/common/messages/Message.h"
#include "src/common/tracing/Trace.h"
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
	LTFSDMErr rc = LTFSDMErr::LTFSDM_OK ;

	try {
		LTFSDM::init();
	}
	catch(...) {
		rc = LTFSDMErr::LTFSDM_GENERAL_ERROR;
		goto cleanup;
	}

	traceObject.setTrclevel(Trace::little);

	if ( argc < 2 ) {
		openLTFSCommand = new HelpCommand();
		openLTFSCommand->doCommand(argc, argv);
		rc = LTFSDMErr::LTFSDM_GENERAL_ERROR;
		goto cleanup;
	}

	command = std::string(argv[1]);

	TRACE(Trace::little, argc);
	TRACE(Trace::little, command.c_str());

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
			INFO(LTFSDMC0011E);
			InfoCommand().printUsage();
			rc =  LTFSDMErr::LTFSDM_GENERAL_ERROR;
			goto cleanup;
		}
		argc--;
		argv++;
		command = std::string(argv[1]);
		TRACE(Trace::little, command.c_str());
		if      ( InfoRequestsCommand().compare(command) ) {
			openLTFSCommand = new InfoRequestsCommand();
		}
		else if ( InfoFilesCommand().compare(command) ) {
			openLTFSCommand = new InfoFilesCommand();
		}
		else {
			MSG(LTFSDMC0012E, command.c_str());
			openLTFSCommand = new HelpCommand();
			rc = LTFSDMErr::LTFSDM_GENERAL_ERROR;
			goto cleanup;
		}
	}
	else {
		MSG(LTFSDMC0005E, command.c_str());
		openLTFSCommand = new HelpCommand();
		rc = LTFSDMErr::LTFSDM_GENERAL_ERROR;
		goto cleanup;
	}

	TRACE(Trace::little, openLTFSCommand);

	argc--;
	argv++;

	if (argc > 1) {
		TRACE(Trace::little, argc);
		TRACE(Trace::little, argv[1]);
	}

	try {
		openLTFSCommand->doCommand(argc, argv);
	}
	catch(LTFSDMErr err) {
		rc = err;
		goto cleanup;
	}

cleanup:
	if (openLTFSCommand)
		delete(openLTFSCommand);

	return (int) rc;
}
