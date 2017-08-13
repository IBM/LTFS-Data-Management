#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/resource.h>
#include <unistd.h>

#include <string>
#include <set>
#include <vector>
#include <list>
#include <sstream>
#include <exception>

#include "src/common/exception/OpenLTFSException.h"
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
    INFO(LTFSDMC0056I);
}

void InfoFsCommand::talkToBackend(std::stringstream *parmList)

{
}

void InfoFsCommand::doCommand(int argc, char **argv)
{
    std::set<std::string> fsList;
    std::set<std::string>::iterator it;
    Connector connector(false);

    processOptions(argc, argv);

    if (argc > 1) {
        printUsage();
        THROW(Error::LTFSDM_GENERAL_ERROR);
    }

    fsList = LTFSDM::getFs();

    for (it = fsList.begin(); it != fsList.end(); ++it) {
        try {
            FsObj fileSystem(*it);
            if (fileSystem.isFsManaged()) {
                INFO(LTFSDMC0057I, *it);
            }
        } catch (const OpenLTFSException& e) {
            switch (e.getError()) {
                case Error::LTFSDM_FS_CHECK_ERROR:
                    MSG(LTFSDMC0058E, *it);
                    break;
            }
        } catch (const std::exception& e) {
        }
    }

}
