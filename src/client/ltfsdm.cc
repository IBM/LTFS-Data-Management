#include <string>
#include "src/common/messages/messages.h"
#include "src/common/tracing/trace.h"

#include "Operation.h"
#include "Migration.h"
#include "Recall.h"
#include "List.h"

void printUsage()

{
	MSG_INFO(OLTFSC0004I);
}


bool compareParameter(const char *tocomparewith, const char *parameter)

{
    if(strncmp(tocomparewith, parameter, strlen(tocomparewith)))
        return false;

    if(strnlen(parameter, strlen(tocomparewith) + 1) != strlen(tocomparewith))
        return false;

    return true;
}

int main(int argc, char *argv[])

{
	Operation *operation = NULL;

	if ( argc < 2 ) {
		printUsage();
		return -1;
	}

	char *command = (char *) argv[1];

	TRACE(0, argv[1]);
	TRACE(0, argc);
	TRACE(0, "Hello World!");

	if( compareParameter("migrate", command) ) {
        operation = new Migration();
    }
    else if( compareParameter("recall", command) ) {
        operation = new Recall();
    }
    else if( compareParameter("list", command) ) {
        operation = new List();
    }
	else {
		MSG_OUT(OLTFSC0005E, command);
		printUsage();
		return -1;
	}

	TRACE(0,  operation);

	operation->doOperation(argc, argv);

	return 0;
}
