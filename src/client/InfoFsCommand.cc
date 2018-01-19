#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/resource.h>
#include <unistd.h>
#include <libmount/libmount.h>
#include <blkid/blkid.h>

#include <string>
#include <map>
#include <set>
#include <vector>
#include <list>
#include <sstream>
#include <exception>

#include "src/common/errors/errors.h"
#include "src/common/exception/LTFSDMException.h"
#include "src/common/util/util.h"
#include "src/common/util/FileSystems.h"
#include "src/common/messages/Message.h"
#include "src/common/tracing/Trace.h"
#include "src/common/configuration/Configuration.h"

#include "src/common/comm/ltfsdm.pb.h"
#include "src/common/comm/LTFSDmComm.h"

#include "src/connector/Connector.h"

#include "LTFSDMCommand.h"
#include "InfoFsCommand.h"

/** @page ltfsdm_info_fs ltfsdm info fs
    The ltfsdm info fs command lists all file systems that are managed with LTFS Data Management:

    <tt>@LTFSDMC0056I</tt>

    parameters | description
    ---|---
    - | -

    @bug Does not work currently. This command uses old code to determine the filesystems being managed.

    Example:

    @verbatim
    @endverbatim

    The corresponding class is @ref InfoFsCommand.
 */

void InfoFsCommand::printUsage()
{
    INFO(LTFSDMC0056I);
}

void InfoFsCommand::talkToBackend(std::stringstream *parmList)

{
}

void InfoFsCommand::doCommand(int argc, char **argv)
{
    Connector connector(false);

    processOptions(argc, argv);

    if (argc > 1) {
        printUsage();
        THROW(Error::GENERAL_ERROR);
    }

    try {
        FileSystems fss;
        for (FileSystems::fsinfo& fs : fss.getAll()) {
            try {
                FsObj fileSystem(fs.target);
                if (fileSystem.isFsManaged()) {
                    INFO(LTFSDMC0057I, fs.target);
                }
            } catch (const LTFSDMException& e) {
                if (e.getError() == Error::FS_CHECK_ERROR) {
                    MSG(LTFSDMC0058E, fs.target);
                    break;
                }
            } catch (const std::exception& e) {
                TRACE(Trace::error, e.what());
                THROW(Error::GENERAL_ERROR);
            }
        }
    } catch (const std::exception& e) {
        TRACE(Trace::error, e.what());
        THROW(Error::GENERAL_ERROR);
    }
}
