#include <sys/resource.h>

#include <string>
#include <sstream>
#include <list>

#include "src/common/messages/Message.h"
#include "src/common/tracing/Trace.h"
#include "src/common/errors/errors.h"

#include "src/common/comm/ltfsdm.pb.h"
#include "src/common/comm/LTFSDmComm.h"

#include "OpenLTFSCommand.h"
#include "StartCommand.h"
#include "StopCommand.h"
#include "MigrationCommand.h"
#include "RecallCommand.h"
#include "AddCommand.h"
#include "StatusCommand.h"
#include "InfoCommand.h"
#include "InfoRequestsCommand.h"
#include "InfoFilesCommand.h"
#include "InfoJobsCommand.h"
#include "InfoFsCommand.h"
#include "StatusCommand.h"
#include "InfoDrivesCommand.h"
#include "InfoTapesCommand.h"
#include "PoolCommand.h"
#include "PoolCreateCommand.h"
#include "PoolDeleteCommand.h"
#include "PoolAddCommand.h"
#include "PoolRemoveCommand.h"
#include "InfoPoolsCommand.h"
#include "RetrieveCommand.h"
#include "HelpCommand.h"

void HelpCommand::printUsage()
{
	INFO(LTFSDMC0008I);
	INFO(LTFSDMC0020I);
	INFO(LTFSDMC0073I);
}

void HelpCommand::doCommand(int argc, char **argv)

{
	OpenLTFSCommand *openLTFSCommand = NULL;
	std::string command;

	if ( argc == 1 ) {
		printUsage();
		return;
	}

	TRACE(Trace::normal, argc);
	TRACE(Trace::normal, command.c_str());

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
	else if ( AddCommand().compare(command) ) {
		openLTFSCommand = new AddCommand();
	}
	else if ( StatusCommand().compare(command) ) {
		openLTFSCommand = new StatusCommand();
	}
	else if ( RetrieveCommand().compare(command) ) {
		openLTFSCommand = new RetrieveCommand();
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
			TRACE(Trace::normal, command.c_str());
			if      ( InfoRequestsCommand().compare(command) ) {
				openLTFSCommand = new InfoRequestsCommand();
			}
			else if ( InfoFilesCommand().compare(command) ) {
				openLTFSCommand = new InfoFilesCommand();
			}
			else if ( InfoJobsCommand().compare(command) ) {
				openLTFSCommand = new InfoJobsCommand();
			}
			else if ( InfoFsCommand().compare(command) ) {
				openLTFSCommand = new InfoFsCommand();
			}
			else if ( InfoDrivesCommand().compare(command) ) {
				openLTFSCommand = new InfoDrivesCommand();
			}
			else if ( InfoTapesCommand().compare(command) ) {
				openLTFSCommand = new InfoTapesCommand();
			}
			else if ( InfoPoolsCommand().compare(command) ) {
				openLTFSCommand = new InfoPoolsCommand();
			}
			else {
				openLTFSCommand = new InfoCommand();
			}
		}
	}
	else if ( PoolCommand().compare(command) ) {
		if ( argc < 3 ) {
			openLTFSCommand = new PoolCommand();
		}
		else {
			command = std::string(argv[2]);
			TRACE(Trace::normal, command.c_str());
			if      ( PoolCreateCommand().compare(command) ) {
				openLTFSCommand = new PoolCreateCommand();
			}
			else if ( PoolDeleteCommand().compare(command) ) {
				openLTFSCommand = new PoolDeleteCommand();
			}
			else if ( PoolAddCommand().compare(command) ) {
				openLTFSCommand = new PoolAddCommand();
			}
			else if ( PoolRemoveCommand().compare(command) ) {
				openLTFSCommand = new PoolRemoveCommand();
			}
			else {
				openLTFSCommand = new PoolCommand();
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
