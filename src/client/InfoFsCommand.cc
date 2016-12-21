#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include <string>
#include <set>

#include "src/common/util/util.h"
#include "src/common/messages/Message.h"
#include "src/common/tracing/Trace.h"
#include "src/common/errors/errors.h"

#include "src/common/comm/ltfsdm.pb.h"
#include "src/common/comm/LTFSDmComm.h"

#include "src/connector/Connector.h"

#include "OpenLTFSCommand.h"
#include "InfoFsCommand.h"

void InfoFsCommand::printUsage()
{
	INFO(LTFSDMC0010I);
}

void InfoFsCommand::talkToBackend(std::stringstream *parmList)

{
}

void InfoFsCommand::doCommand(int argc, char **argv)
{
	std::set<std::string> fsList;
	std::set<std::string>::iterator it;
	Connector connector(false);

	if ( argc > 1 ) {
		INFO(LTFSDMC0018E);
		throw(Error::LTFSDM_GENERAL_ERROR);

	}

	fsList = LTFSDM::getFs();

	for ( it = fsList.begin(); it != fsList.end(); ++it ) {
		try {
			FsObj fileSystem(*it);
			if ( fileSystem.isFsManaged() ) {
				INFO(LTFSDMC0057I, *it);
			}
		}
		catch ( int error ) {
			switch ( error ) {
				case Error::LTFSDM_FS_CHECK_ERROR:
					MSG(LTFSDMC0058E, *it);
					break;
			}
		}
	}

}
