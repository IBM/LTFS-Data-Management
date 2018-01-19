#include <unistd.h>
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
#include "RecallCommand.h"

/** @page ltfsdm_recall ltfsdm recall
    The ltfsdm recall command is used to selectively recall one or more files from tape.

    <tt>@LTFSDMC0002I</tt>

    parameters | description
    ---|---
    -r | to recall files to resident state, without specifying this option files get recalled to premigrated state
    -n \<request number\> | attach to an ongoing recall request to see its progress
    \<file name\> | a set of file names of files to be recalled
    -f \<file list\> | a file name containing a list of files to be recalled

    Example:

    @verbatim
    [root@visp sdir]# find dir.* -type f |ltfsdm recall -r -f -
    --- sending completed within 42 seconds ---
                   resident  premigrated     migrated       failed
    [00:01:01]            1            0        99999            0
    [00:01:11]        17108            0        82892            0
    [00:01:21]        36376            0        63624            0
    [00:01:31]        55464            0        44536            0
    [00:01:41]        74717            0        25283            0
    [00:01:51]        93946            0         6054            0
    [00:01:56]       100000            0            0            0
    @endverbatim

    The corresponding class is @ref RecallCommand.

    @page recall_processing recall processing

    The following is a summary of the recall command processing:

    @code
    RecallCommand::doCommand
        LTFSDMCommand::processOptions
        LTFSDMCommand::checkOptions
        if filename arguments
            create stream containing file names
        LTFSDMCommand::isValidRegularFile
        RecallCommand::talkToBackend
            create selrecrequest message
            LTFSDmCommClient::send
            LTFSDmCommClient::recv
            evaluate response
            LTFSDMCommand::sendObjects
                while filenames to send
                    create sendobjecs message
                    LTFSDmCommClient::send
                    LTFSDmCommClient::recv
                    evaluate response
            LTFSDMCommand::queryResults
                do
                    create reqstatusrequest message
                    LTFSDmCommClient::send
                    LTFSDmCommClient::recv
                    print progress
                until not done
    @endcode

    After the option processing is done it is evaluated in which way the
    file names are provided to the command. There are three different
    possibilities:

    - the file names are provided as arguments to the command
    - a file list is provided containing the file names
    - a "-" is provided as file list name and the file names are provided by stdin

    If a file list is provided it is checked if it is a valid regular file.

    The further processing is performed by communicating with the backend. The
    three steps that are performed are the following

    - general selective recall information is sent to the backend
    - the file names are send to the backend
    - the progress or results are queried
 */

void RecallCommand::printUsage()
{
    INFO(LTFSDMC0002I);
}

void RecallCommand::talkToBackend(std::stringstream *parmList)

{
    try {
        connect();
    } catch (const std::exception& e) {
        MSG(LTFSDMC0026E);
        return;
    }

    LTFSDmProtocol::LTFSDmSelRecRequest *recreq =
            commCommand.mutable_selrecrequest();

    recreq->set_key(key);
    recreq->set_reqnumber(requestNumber);
    recreq->set_pid(getpid());

    if (recToResident == true)
        recreq->set_state(LTFSDmProtocol::LTFSDmSelRecRequest::RESIDENT);
    else
        recreq->set_state(LTFSDmProtocol::LTFSDmSelRecRequest::PREMIGRATED);

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

    const LTFSDmProtocol::LTFSDmSelRecRequestResp recreqresp =
            commCommand.selrecrequestresp();

    switch (recreqresp.error()) {
        case static_cast<long>(Error::OK):
            if (getpid() != recreqresp.pid()) {
                MSG(LTFSDMC0036E);
                TRACE(Trace::error, getpid(), recreqresp.pid());
                THROW(Error::GENERAL_ERROR);
            }
            if (requestNumber != recreqresp.reqnumber()) {
                MSG(LTFSDMC0037E);
                TRACE(Trace::error, requestNumber, recreqresp.reqnumber());
                THROW(Error::GENERAL_ERROR);
            }
            break;
        case static_cast<long>(Error::TERMINATING):
            MSG(LTFSDMC0101I);
            THROW(Error::GENERAL_ERROR);
            break;
        default:
            MSG(LTFSDMC0029E);
            THROW(Error::GENERAL_ERROR);
    }

    sendObjects(parmList);

    queryResults();
}

void RecallCommand::doCommand(int argc, char **argv)
{
    std::stringstream parmList;

    if (argc == 1) {
        INFO(LTFSDMC0018E);
        THROW(Error::GENERAL_ERROR);

    }

    processOptions(argc, argv);

    try {
        checkOptions(argc, argv);
    } catch (const std::exception& e) {
        printUsage();
        THROW(Error::GENERAL_ERROR);
    }

    TRACE(Trace::normal, argc, optind);
    traceParms();

    if (!fileList.compare("")) {
        for (int i = optind; i < argc; i++) {
            parmList << argv[i] << std::endl;
        }
    }

    isValidRegularFile();

    talkToBackend(&parmList);
}
