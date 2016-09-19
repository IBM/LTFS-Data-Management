#include <unistd.h>
#include <string>

#include "src/common/messages/Message.h"
#include "src/common/tracing/Trace.h"
#include "src/common/errors/errors.h"

#include "OpenLTFSCommand.h"
#include "InfoFilesCommand.h"

void InfoFilesCommand::printUsage()
{
	MSG_INFO(OLTFSC0010I);
}

void InfoFilesCommand::doCommand(int argc, char **argv)
{
	if ( argc == 1 ) {
		MSG_INFO(OLTFSC0018E);
		goto error;
	}

	processOptions(argc, argv);

	if ( fileList.compare("") && directoryName.compare("") ) {
		MSG_INFO(OLTFSC0015E);
		goto error;
	}

	if (optind != argc) {
		if (fileList.compare("")) {
			MSG_INFO(OLTFSC0016E);
			goto error;
		}
		if (directoryName.compare("")) {
			MSG_INFO(OLTFSC0017E);
			goto error;
		}
	}
	else if ( !fileList.compare("") && !directoryName.compare("") ) {
		// a least a file or directory needs to be specified
		MSG_INFO(OLTFSC0019E);
		goto error;
	}

	TRACE(0, fileList);
	TRACE(0, directoryName);

	return;

error:
	printUsage();
	throw(OLTFSErr::OLTFS_GENERAL_ERROR);
}
