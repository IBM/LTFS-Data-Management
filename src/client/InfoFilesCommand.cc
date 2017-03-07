#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <sys/resource.h>

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
	std::stringstream tapeIds;
	FsObj::mig_attr_t attr;

	if ( argc == 1 ) {
		INFO(LTFSDMC0018E);
		throw(Error::LTFSDM_GENERAL_ERROR);

	}

	processOptions(argc, argv);

	checkOptions(argc, argv);

	TRACE(Trace::little, argc);
	TRACE(Trace::little, optind);
	traceParms();

	if ( !fileList.compare("") ) {
		for ( int i=optind; i<argc; i++ ) {
			parmList << argv[i] << std::endl;
		}
	}

	isValidRegularFile();

	if ( fileList.compare("") ) {
		fileListStrm.open(fileList);
		input =  dynamic_cast<std::istream*>(&fileListStrm);
	}
	else {
		input = dynamic_cast<std::istream*>(&parmList);
	}

	INFO(LTFSDMC0047I);

	while ( std::getline(*input, line) ) {
		try {
			file_name = canonicalize_file_name(line.c_str());
			if ( file_name == NULL ) {
				continue;
			}
			FsObj fso(file_name);
			statbuf = fso.stat();
			attr = fso.getAttribute();
			tapeIds.str("");
			tapeIds.clear();
			if ( attr.copies == 0 ) {
				tapeIds << "-";
			}
			else {
				for (int i=0; i<attr.copies; i++) {
					if ( i != 0 ) tapeIds << ",";
					tapeIds << attr.tapeId[i];
				}
			}
			if ( !S_ISREG(statbuf.st_mode ) ) {
				INFO(LTFSDMC0049I, '-', statbuf.st_size, statbuf.st_blocks, tapeIds.str(), file_name);
			}
			else {
				switch(fso.getMigState()) {
					case FsObj::MIGRATED: migstate='m'; break;
					case FsObj::PREMIGRATED: migstate='p'; break;
					case FsObj::RESIDENT: migstate='r'; break;
					default: migstate=' ';
				}
				INFO(LTFSDMC0049I, migstate, statbuf.st_size, statbuf.st_blocks, tapeIds.str(), file_name);
			}
		}
		catch ( int error ) {
			if ( stat(file_name, &statbuf) == -1 )
				continue;
			INFO(LTFSDMC0049I, '-', statbuf.st_size, statbuf.st_blocks, '-', file_name);
		}
	}
}
