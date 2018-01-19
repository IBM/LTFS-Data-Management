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
#include "InfoJobsCommand.h"

/** @page ltfsdm_info_jobs ltfsdm info jobs
    The ltfsdm info jobs command lists all jobs that are currently processed by the backend.

    <tt>@LTFSDMC0059I</tt>

    parameters | description
    ---|---
    -n \<request number\> | restrict the jobs to be displayed to a certain request

    Example:

    @verbatim
    [root@visp ~]# ltfsdm info jobs -n 19
    operation            state                request number       tape pool            tape id              size                 file name
    migration            premigrating         19                   pool1                D01301L5             32768                /mnt/lxfs/test2/file.0
    migration            premigrating         19                   pool1                D01301L5             32768                /mnt/lxfs/test2/file.1
    migration            premigrating         19                   pool1                D01301L5             32768                /mnt/lxfs/test2/file.2
    migration            premigrating         19                   pool1                D01301L5             32768                /mnt/lxfs/test2/file.3
    migration            premigrating         19                   pool1                D01301L5             32768                /mnt/lxfs/test2/file.4
    migration            premigrating         19                   pool1                D01301L5             32768                /mnt/lxfs/test2/file.5
    migration            premigrating         19                   pool1                D01301L5             32768                /mnt/lxfs/test2/file.6
    migration            premigrating         19                   pool1                D01301L5             32768                /mnt/lxfs/test2/file.7
    migration            premigrating         19                   pool1                D01301L5             32768                /mnt/lxfs/test2/file.8
    migration            premigrating         19                   pool1                D01301L5             32768                /mnt/lxfs/test2/file.9
    @endverbatim

    The corresponding class is @ref InfoJobsCommand.
 */

void InfoJobsCommand::printUsage()
{
    INFO(LTFSDMC0059I);
}

void InfoJobsCommand::doCommand(int argc, char **argv)
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

    LTFSDmProtocol::LTFSDmInfoJobsRequest *infojobs =
            commCommand.mutable_infojobsrequest();

    infojobs->set_key(key);
    infojobs->set_reqnumber(reqOfInterest);

    try {
        commCommand.send();
    } catch (const std::exception& e) {
        MSG(LTFSDMC0027E);
        THROW(Error::GENERAL_ERROR);
    }

    INFO(LTFSDMC0062I);
    int recnum;

    do {
        try {
            commCommand.recv();
        } catch (const std::exception& e) {
            MSG(LTFSDMC0028E);
            THROW(Error::GENERAL_ERROR);
        }

        const LTFSDmProtocol::LTFSDmInfoJobsResp infojobsresp =
                commCommand.infojobsresp();
        std::string operation = infojobsresp.operation();
        std::string filename = infojobsresp.filename();
        recnum = infojobsresp.reqnumber();
        std::string pool = infojobsresp.pool();
        unsigned long size = infojobsresp.filesize();
        std::string tapeid = infojobsresp.tapeid();
        std::string state = infojobsresp.state();
        if (recnum != Const::UNSET)
            INFO(LTFSDMC0063I, operation, state, recnum, pool, tapeid, size,
                    filename);

    } while (!exitClient && recnum != Const::UNSET);

    return;
}
