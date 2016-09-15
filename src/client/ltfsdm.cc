#include <string>
#include "src/common/messages/messages.h"
#include "src/common/tracing/trace.h"

#include "Operation.h"
#include "Start.h"
#include "Stop.h"
#include "Migration.h"
#include "Recall.h"
#include "Help.h"
#include "InfoRequests.h"
#include "InfoFiles.h"

bool compareParameter(const char *tocomparewith, const char *parameter)

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
		return -1;
	}

	char *command = (char *) argv[1];

	TRACE(0, argc);
	TRACE(0, command);

 	if (      compareParameter("start",   command) ) { operation = new Start(); }
	else if ( compareParameter("stop",    command) ) { operation = new Stop(); }
 	else if ( compareParameter("migrate", command) ) { operation = new Migration(); }
    else if ( compareParameter("recall",  command) ) { operation = new Recall(); }
    else if ( compareParameter("help",    command) ) { operation = new Help(); }
	else if ( compareParameter("info",    command) ) {
		if ( argc < 3 ) {
			MSG_OUT(OLTFSC0011E);
			return -1;
		}
		command = (char *) argv[2];
		TRACE(0, command);
		if(       compareParameter("requests", command) ) { operation = new InfoRequests(); }
		else if ( compareParameter("files",    command) ) { operation = new InfoFiles(); }
		else {
			MSG_OUT(OLTFSC0012E, command);
			operation = new Help();
			return -1;
		}
	}
	else {
		MSG_OUT(OLTFSC0005E, command);
		operation = new Help();
		return -1;
	}

	TRACE(0, operation);

	operation->doOperation(argc, argv);

	return 0;
}
