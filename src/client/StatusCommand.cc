#include <sys/resource.h>

#include <string>
#include <list>
#include <sstream>
#include <exception>

#include "src/common/errors/errors.h"
#include "src/common/exception/LTFSDMException.h"
#include "src/common/messages/Message.h"
#include "src/common/tracing/Trace.h"

#include "src/common/comm/ltfsdm.pb.h"
#include "src/common/comm/LTFSDmComm.h"

#include "LTFSDMCommand.h"
#include "StatusCommand.h"

/** @page ltfsdm_status ltfsdm status
    The ltfsdm status command provides the status of the LTFS Data Management service.

    <tt>@LTFSDMC0030I</tt>

    parameters | description
    ---|---
    - | -

    Example:

    @verbatim
    [root@visp ~]# ltfsdm status
    LTFSDMC0032I(0068): The LTFS Data Management server process is operating with pid  13378.
    @endverbatim

    The corresponding class is @ref StatusCommand.
 */

void StatusCommand::printUsage()
{
    INFO(LTFSDMC0030I);
}

void StatusCommand::doCommand(int argc, char **argv)
{
    int pid;

    processOptions(argc, argv);

    if (argc > 1) {
        printUsage();
        THROW(Error::GENERAL_ERROR);
    }

    try {
        connect();
    } catch (const std::exception& e) {
        MSG(LTFSDMC0026E);
        return;
    }

    TRACE(Trace::normal, requestNumber);

    LTFSDmProtocol::LTFSDmStatusRequest *statusreq =
            commCommand.mutable_statusrequest();
    statusreq->set_key(key);
    statusreq->set_reqnumber(requestNumber);

    try {
        commCommand.send();
    } catch (const std::exception& e) {
        MSG(LTFSDMC0027E);
        THROW(Error::GENERAL_ERROR);
    }

    try {
        commCommand.recv();
    } catch (const std::exception& e) {
        MSG(LTFSDMC0028E);
        THROW(Error::GENERAL_ERROR);
    }

    const LTFSDmProtocol::LTFSDmStatusResp statusresp =
            commCommand.statusresp();

    if (statusresp.success() == true) {
        pid = statusresp.pid();
        MSG(LTFSDMC0032I, pid);
    } else {
        MSG(LTFSDMC0029E);
        THROW(Error::GENERAL_ERROR);
    }
}
