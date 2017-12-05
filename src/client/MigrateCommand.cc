#include <unistd.h>
#include <sys/resource.h>

#include <string>
#include <sstream>
#include <list>
#include <exception>

#include "src/common/errors/errors.h"
#include "src/common/exception/OpenLTFSException.h"
#include "src/common/messages/Message.h"
#include "src/common/tracing/Trace.h"

#include "src/common/comm/ltfsdm.pb.h"
#include "src/common/comm/LTFSDmComm.h"

#include "OpenLTFSCommand.h"
#include "MigrateCommand.h"

/** @page ltfsdm_migrate ltfsdm migrate
    The ltfsdm recall command selectively recall one or more files.

    <tt>@LTFSDMC0001I</tt>

    parameters | description
    ---|---
    -p | to premigrate files, without specifying this option files get migrated
    -P \<pool list: 'pool1,pool2,pool3'\> | a file can be migrated up to three different tape storage pools in parallel, at least one tape storage pool needs to be specified
    -n \<request number\> | attach to an ongoing migration request to see its progress
    \<file name\> | a set of file names of files to be migrated
    -f \<file list\> | a file name containing a list of files to be migrated

    Example:

    @verbatim
    [root@visp sdir]# find dir.* -type f |ltfsdm migrate -P pool1 -f -
    --- sending completed within 32 seconds ---
                   resident  premigrated     migrated       failed
    [00:00:58]        99999            1            0            0
    [00:01:08]        68937        31063            0            0
    [00:01:18]        37745        62255            0            0
    [00:01:28]         6558        93442            0            0
    [00:01:38]            0       100000            0            0
    [00:01:48]            0        36155        63845            0
    [00:01:54]            0            0       100000            0
    @endverbatim

    The responsible class is @ref MigrateCommand.

    @page migrate_processing migrate processing

    The following is a summary of the migrate command processing:

    @code
    MigrateCommand::doCommand
        OpenLTFSCommand::processOptions
        OpenLTFSCommand::checkOptions
        if filename arguments
            create stream containing file names
        OpenLTFSCommand::isValidRegularFile
        MigrateCommand::talkToBackend
            create migrequest message
            LTFSDmCommClient::send
            LTFSDmCommClient::recv
            evaluate response
            OpenLTFSCommand::sendObjects
                while filenames to send
                    create sendobjecs message
                    LTFSDmCommClient::send
                    LTFSDmCommClient::recv
                    evaluate response
            OpenLTFSCommand::queryResults
                do
                    create reqstatusrequest message
                    LTFSDmCommClient::send
                    LTFSDmCommClient::recv
                    print progress
                until not done
    @endcode

    After processing the migration options it is evaluated how the file names
    are provided. There are three different possibilities

    - the file names are provided as arguments to the command
    - a file list is provided containing the file names
    - a "-" is provided as file list name and the file names are provided by stdin

    If the file names are provided as arguments a stream is created containing
    all the names. If a file list is provided it is checked if it is a valid
    regular file.

    The further processing is performed by communicating with the backend. The
    three steps that are performed are the following

    - general migration information is sent to the backend
    - the file names are send to the backend
    - the progress or results are queried

 */

void MigrateCommand::printUsage()
{
    INFO(LTFSDMC0001I);
}

void MigrateCommand::talkToBackend(std::stringstream *parmList)

{
    try {
        connect();
    } catch (const std::exception& e) {
        MSG(LTFSDMC0026E);
        return;
    }

    LTFSDmProtocol::LTFSDmMigRequest *migreq = commCommand.mutable_migrequest();

    migreq->set_key(key);
    migreq->set_reqnumber(requestNumber);
    migreq->set_pid(getpid());
    migreq->set_pools(poolNames);

    if (preMigrate == true)
        migreq->set_state(LTFSDmProtocol::LTFSDmMigRequest::PREMIGRATED);
    else
        migreq->set_state(LTFSDmProtocol::LTFSDmMigRequest::MIGRATED);

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

    const LTFSDmProtocol::LTFSDmMigRequestResp migreqresp =
            commCommand.migrequestresp();

    switch (migreqresp.error()) {
        case static_cast<long>(Error::OK):
            if (getpid() != migreqresp.pid()) {
                MSG(LTFSDMC0036E);
                TRACE(Trace::error, getpid(), migreqresp.pid());
                THROW(Error::GENERAL_ERROR);
            }
            if (requestNumber != migreqresp.reqnumber()) {
                MSG(LTFSDMC0037E);
                TRACE(Trace::error, requestNumber, migreqresp.reqnumber());
                THROW(Error::GENERAL_ERROR);
            }
            break;
        case static_cast<long>(Error::WRONG_POOLNUM):
            MSG(LTFSDMS0063E);
            THROW(Error::GENERAL_ERROR);
            break;
        case static_cast<long>(Error::NOT_ALL_POOLS_EXIST):
            MSG(LTFSDMS0064E);
            THROW(Error::GENERAL_ERROR);
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

void MigrateCommand::doCommand(int argc, char **argv)
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
