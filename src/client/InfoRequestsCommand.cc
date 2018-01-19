#include <sys/resource.h>

#include <unistd.h>
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
#include "InfoRequestsCommand.h"

/** @page ltfsdm_info_requests ltfsdm info requests
    The ltfsdm info request command lists all LTFS Data Management requests
    and their corresponding status.

    <tt>@LTFSDMC0009I</tt>

    parameters | description
    ---|---
    -n \<request number\> | request number for a specific request to see the information

    Example:

    @verbatim
    [root@visp ~]# ltfsdm info requests -n 28
    operation            state                request number       tape pool            tape id              target state
    migration            in progress          28                   pool1                D01301L5             in progress
    @endverbatim

    The corresponding class is @ref InfoRequestsCommand.
 */

void InfoRequestsCommand::printUsage()
{
    INFO(LTFSDMC0009I);
}

void InfoRequestsCommand::doCommand(int argc, char **argv)
{
    long reqOfInterest;

    processOptions(argc, argv);

    TRACE(Trace::normal, *argv, argc, optind);

    if (argc != optind) {
        printUsage();
        THROW(Error::GENERAL_ERROR);
    } else if (requestNumber < Const::UNSET) {
        printUsage();
        THROW(Error::GENERAL_ERROR);
    }

    reqOfInterest = requestNumber;

    try {
        connect();
    } catch (const std::exception& e) {
        MSG(LTFSDMC0026E);
        return;
    }

    LTFSDmProtocol::LTFSDmInfoRequestsRequest *inforeqs =
            commCommand.mutable_inforequestsrequest();

    inforeqs->set_key(key);
    inforeqs->set_reqnumber(reqOfInterest);

    try {
        commCommand.send();
    } catch (const std::exception& e) {
        MSG(LTFSDMC0027E);
        THROW(Error::GENERAL_ERROR);
    }

    INFO(LTFSDMC0060I);
    int recnum;

    do {
        try {
            commCommand.recv();
        } catch (const std::exception& e) {
            MSG(LTFSDMC0028E);
            THROW(Error::GENERAL_ERROR);
        }

        const LTFSDmProtocol::LTFSDmInfoRequestsResp inforeqsresp =
                commCommand.inforequestsresp();
        std::string operation = inforeqsresp.operation();
        recnum = inforeqsresp.reqnumber();
        std::string tapeid = inforeqsresp.tapeid();
        std::string tstate = inforeqsresp.targetstate();
        std::string state = inforeqsresp.state();
        std::string pool = inforeqsresp.pool();
        if (recnum != Const::UNSET)
            INFO(LTFSDMC0061I, operation, state, recnum, pool, tapeid, tstate);

    } while (recnum != Const::UNSET);

    return;
}
