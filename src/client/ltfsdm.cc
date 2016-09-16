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
#include "InfoRequests.h"
#include "InfoFiles.h"

int main(int argc, char *argv[])

{
	Operation *operation = NULL;

	if ( argc < 2 ) {
		operation = new Help();
		operation->doOperation(argc, argv);
		return (int) OLTFSErr::OLTFS_GENERAL_ERROR;
	}

	char *command = (char *) argv[1];

	TRACE(0, argc);
	TRACE(0, command);

 	if      ( !std::string("help").compare(command)    ) { operation = new Start(); }
	else if ( !std::string("stop").compare(command)    ) { operation = new Stop(); }
 	else if ( !std::string("migrate").compare(command) ) { operation = new Migration(); }
    else if ( !std::string("recall").compare(command)  ) { operation = new Recall(); }
    else if ( !std::string("help").compare(command)    ) { operation = new Help(); }
	else if ( !std::string("info").compare(command)    ) {
		if ( argc < 3 ) {
			MSG_OUT(OLTFSC0011E);
			return (int) OLTFSErr::OLTFS_GENERAL_ERROR;
		}
		argc--;
		argv++;
		command = (char *) argv[1];
		TRACE(0, command);
		if      ( !std::string("requests").compare(command) ) { operation = new InfoRequests(); }
		else if ( !std::string("files").compare(command)    ) { operation = new InfoFiles(); }
		else {
			MSG_OUT(OLTFSC0012E, command);
			operation = new Help();
			return (int) OLTFSErr::OLTFS_GENERAL_ERROR;
		}
	}
	else {
		MSG_OUT(OLTFSC0005E, command);
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
