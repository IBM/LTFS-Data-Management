#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string>

#include "src/common/messages/Message.h"
#include "src/common/tracing/Trace.h"
#include "src/common/errors/errors.h"

#include "src/common/comm/ltfsdm.pb.h"
#include "src/common/comm/LTFSDmComm.h"

#include "src/connector/Connector.h"

#include "OpenLTFSCommand.h"
#include "InfoFilesCommand.h"

void InfoFilesCommand::printUsage()
{
	INFO(LTFSDMC0010I);
}

void InfoFilesCommand::talkToBackend(std::stringstream *parmList)

{
}

void InfoFilesCommand::doCommand(int argc, char **argv)
{
	std::stringstream parmList;
	Connector connector(false);
	struct stat statbuf;
	char migstate;
	std::istream *input;
	std::string line;
	char *file_name;

	if ( argc == 1 ) {
		INFO(LTFSDMC0018E);
		throw(Error::LTFSDM_GENERAL_ERROR);

	}

	processOptions(argc, argv);

	checkOptions(argc, argv);

	TRACE(Trace::little, argc);
	TRACE(Trace::little, optind);
	traceParms();

	if ( !fileList.compare("") && !directoryName.compare("") ) {
		for ( int i=optind; i<argc; i++ ) {
			parmList << argv[i] << std::endl;
		}
	}
	else if ( directoryName.compare("") ) {
		parmList << directoryName << std::endl;
	}

	isValidRegularFile();

	if ( fileList.compare("") ) {
		fileListStrm.open(fileList);
		input =  dynamic_cast<std::istream*>(&fileListStrm);
	}
	else {
		input = dynamic_cast<std::istream*>(&parmList);
	}

	INFO(LTFSDMC0048I);

	while ( std::getline(*input, line) ) {
		try {
			file_name = canonicalize_file_name(line.c_str());
			if ( file_name == NULL ) {
				INFO(LTFSDMC0049I, line);
				continue;
			}
			FsObj fso(file_name);
			statbuf = fso.stat();
			switch(fso.getMigState()) {
				case FsObj::MIGRATED: migstate='m'; break;
				case FsObj::PREMIGRATED: migstate='p'; break;
				case FsObj::RESIDENT: migstate='r'; break;
				default: migstate=' ';
			}
			INFO(LTFSDMC0050I, migstate, statbuf.st_size, statbuf.st_blocks, file_name);
		}
		catch ( int error ) {
			MSG(LTFSDMC0047I, file_name);
		}
	}
}
