#include <sys/resource.h>

#include <unistd.h>
#include <string>
#include <list>
#include <sstream>
#include <exception>

#include "src/common/exception/OpenLTFSException.h"
#include "src/common/messages/Message.h"
#include "src/common/tracing/Trace.h"
#include "src/common/errors/errors.h"

#include "src/common/comm/ltfsdm.pb.h"
#include "src/common/comm/LTFSDmComm.h"

#include "OpenLTFSCommand.h"
#include "InfoJobsCommand.h"

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
        throw(EXCEPTION(Error::LTFSDM_GENERAL_ERROR));
    } else if (requestNumber < Const::UNSET) {
        printUsage();
        throw(EXCEPTION(Error::LTFSDM_GENERAL_ERROR));
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
        throw(EXCEPTION(Error::LTFSDM_GENERAL_ERROR));
    }

    INFO(LTFSDMC0062I);
    int recnum;

    do {
        try {
            commCommand.recv();
        } catch (const std::exception& e) {
            MSG(LTFSDMC0028E);
            throw(EXCEPTION(Error::LTFSDM_GENERAL_ERROR));
        }

        const LTFSDmProtocol::LTFSDmInfoJobsResp infojobsresp =
                commCommand.infojobsresp();
        std::string operation = infojobsresp.operation();
        std::string filename = infojobsresp.filename();
        recnum = infojobsresp.reqnumber();
        int replnum = infojobsresp.replnumber();
        int size = infojobsresp.filesize();
        std::string tapeid = infojobsresp.tapeid();
        std::string state = infojobsresp.state();
        if (recnum != Const::UNSET)
            INFO(LTFSDMC0063I, operation, state, recnum, replnum, size, tapeid,
                    filename);

    } while (recnum != Const::UNSET);

    return;
}
