#include <string>
#include "src/common/messages/messages.h"
#include "src/common/tracing/trace.h"
#include "src/common/errors/errors.h"

#include "Operation.h"
#include "Start.h"
#include "Stop.h"
#include "Migration.h"
#include "Recall.h"
#include "Help.h"
#include "Info.h"
#include "InfoRequests.h"
#include "InfoFiles.h"

int main(int argc, char *argv[])

{
	Operation *operation = NULL;
	std::string command;

	if ( argc < 2 ) {
		operation = new Help();
		operation->doOperation(argc, argv);
		return (int) OLTFSErr::OLTFS_GENERAL_ERROR;
	}

	command = std::string(argv[1]);

	TRACE(0, argc);
	TRACE(0, command.c_str());

 	if  ( !command.compare(Start::cmd1) ) {
		operation = new Start();
	} else if ( !command.compare(Stop::cmd1) ) {
		operation = new Stop();
	} else if ( !command.compare(Migration::cmd1) ) {
		operation = new Migration();
	} else if ( !command.compare(Recall::cmd1) ) {
		operation = new Recall();
	} else if ( !command.compare(Help::cmd1) ) {
		operation = new Help();
	} else if ( !command.compare(Info::cmd1) ) {
		if ( argc < 3 ) {
			MSG_OUT(OLTFSC0011E);
			return (int) OLTFSErr::OLTFS_GENERAL_ERROR;
		}
		argc--;
		argv++;
		command = std::string(argv[1]);
		TRACE(0, command.c_str());
		if      ( !command.compare(InfoRequests::cmd2) ) {
			operation = new InfoRequests();
		} else if ( !command.compare(InfoFiles::cmd2) ) {
			operation = new InfoFiles();
		} else {
			MSG_OUT(OLTFSC0012E, command.c_str());
			operation = new Help();
			return (int) OLTFSErr::OLTFS_GENERAL_ERROR;
		}
	}
	else {
		MSG_OUT(OLTFSC0005E, command.c_str());
		operation = new Help();
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
		operation->doOperation(argc, argv);
	}
	catch(OLTFSErr err) {
		return (int) err;
	}

	return (int) OLTFSErr::OLTFS_OK;
}
