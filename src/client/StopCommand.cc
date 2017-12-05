#include <fcntl.h>
#include <sys/file.h>
#include <sys/resource.h>

#include <string>
#include <list>
#include <sstream>
#include <exception>

#include "src/common/errors/errors.h"
#include "src/common/exception/OpenLTFSException.h"
#include "src/common/messages/Message.h"
#include "src/common/tracing/Trace.h"

#include "src/common/comm/ltfsdm.pb.h"
#include "src/common/comm/LTFSDmComm.h"

#include "OpenLTFSCommand.h"
#include "StopCommand.h"

/** @page ltfsdm_stop ltfsdm stop
    The ltfsdm stop command stop the OpenLTFS service.

    <tt>@LTFSDMC0007I</tt>

    parameters | description
    ---|---
    -x | force the stop of OpenLTFS even a managed file system is in use

    Example:

    @verbatim
    [root@visp ~]# ltfsdm stop
    The OpenLTFS backend is terminating.
    Waiting for the termination of the OpenLTFS server............
    [root@visp ~]#
    @endverbatim

    The responsible class is @ref StopCommand.

    @page stop_processing stop processing

    The following is a summary of the stop command processing:

    @code
    StopCommand::doCommand
        OpenLTFSCommand::processOptions
        OpenLTFSCommand::connect
            LTFSDmCommClient::connect
            OpenLTFSCommand::getRequestNumber
        do
            create stoprequest message
            LTFSDmCommClient::send
            LTFSDmCommClient::recv
            if stopped == false
                sleep 1
        until stopped == false
        open OpenLTFS lock
        while locking fails
            sleep 1
        unlock lock
    @endcode

    When processing the stop command first stoprequest message is sent to the
    to the backend. This is repeated as long the backend does not respond with
    success message. Thereafter the server lock is tried to acquire to see
    that the server process is finally gone. The backend holds a lock all the
    time it is operating.
 */

void StopCommand::printUsage()
{
    INFO(LTFSDMC0007I);
}

void StopCommand::doCommand(int argc, char **argv)
{
    int lockfd;
    bool finished = false;

    processOptions(argc, argv);

    if (argc > 2) {
        printUsage();
        THROW(Error::GENERAL_ERROR);
    }

    try {
        connect();
    } catch (const std::exception& e) {
        MSG(LTFSDMC0026E);
        THROW(Error::GENERAL_ERROR);
    }

    TRACE(Trace::normal, requestNumber);

    INFO(LTFSDMC0101I);

    do {
        LTFSDmProtocol::LTFSDmStopRequest *stopreq =
                commCommand.mutable_stoprequest();
        stopreq->set_key(key);
        stopreq->set_reqnumber(requestNumber);
        stopreq->set_forced(forced);
        stopreq->set_finish(false);

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

        const LTFSDmProtocol::LTFSDmStopResp stopresp = commCommand.stopresp();

        finished = stopresp.success();

        if (!finished) {
            INFO(LTFSDMC0103I);
            sleep(1);
        } else {
            break;
        }
    } while (true);

    INFO(LTFSDMC0104I);

    if ((lockfd = open(Const::SERVER_LOCK_FILE.c_str(), O_RDWR | O_CREAT, 0600))
            == -1) {
        MSG(LTFSDMC0033E);
        TRACE(Trace::error, Const::SERVER_LOCK_FILE, errno);
        THROW(Error::GENERAL_ERROR);
    }
    INFO(LTFSDMC0034I);

    while (flock(lockfd, LOCK_EX | LOCK_NB) == -1) {
        if (exitClient)
            break;
        INFO(LTFSDMC0103I);
        sleep(1);
    }

    INFO(LTFSDMC0104I);

    if (flock(lockfd, LOCK_UN) == -1) {
        MSG(LTFSDMC0035E);
    }
}
