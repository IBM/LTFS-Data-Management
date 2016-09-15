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

bool checkParam(const char *tocomparewith, const char *parameter)

{
	if ( std::string(parameter).compare(tocomparewith) )
		return false;
	else
		return true;
}

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

 	if (      checkParam("start",   command) ) { operation = new Start(); }
	else if ( checkParam("stop",    command) ) { operation = new Stop(); }
 	else if ( checkParam("migrate", command) ) { operation = new Migration(); }
    else if ( checkParam("recall",  command) ) { operation = new Recall(); }
    else if ( checkParam("help",    command) ) { operation = new Help(); }
	else if ( checkParam("info",    command) ) {
		if ( argc < 3 ) {
			MSG_OUT(OLTFSC0011E);
			return (int) OLTFSErr::OLTFS_GENERAL_ERROR;
		}
		argc--;
		argv++;
		command = (char *) argv[1];
		TRACE(0, command);
		if(       checkParam("requests", command) ) { operation = new InfoRequests(); }
		else if ( checkParam("files",    command) ) { operation = new InfoFiles(); }
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
