#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/resource.h>
#include <unistd.h>

#include <string>
#include <vector>
#include <list>
#include <sstream>
#include <exception>

#include "src/common/errors/errors.h"
#include "src/common/exception/LTFSDMException.h"
#include "src/common/util/util.h"
#include "src/common/messages/Message.h"
#include "src/common/tracing/Trace.h"

#include "src/common/Version.h"
#include "src/common/comm/ltfsdm.pb.h"
#include "src/common/comm/LTFSDmComm.h"

#include "LTFSDMCommand.h"
#include "VersionCommand.h"

/** @page ltfsdm_version ltfsdm version
    The ltfsdm version command provide the version information of
    the LTFS Data Management software.

    <tt>@LTFSDMC0102I</tt>

    parameters | description
    ---|---
    - | -

    Example:

    @verbatim
    [root@visp ~]# ltfsdm version
    LTFS Data Management version: 0.0.624-master.2017-11-09T10.57.51
    @endverbatim

    The corresponding class is @ref VersionCommand.
 */

void VersionCommand::printUsage()
{
    INFO(LTFSDMC0102I);
}

void VersionCommand::talkToBackend(std::stringstream *parmList)

{
}

void VersionCommand::doCommand(int argc, char **argv)
{
    processOptions(argc, argv);

    if (argc > 1) {
        printUsage();
        THROW(Error::GENERAL_ERROR);
    }

    INFO(LTFSDMX0029I, LTFSDM_VERSION);
}
