#include <sys/resource.h>

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
#include "RetrieveCommand.h"

void RetrieveCommand::printUsage()
{
    INFO(LTFSDMC0093I);
}

void RetrieveCommand::doCommand(int argc, char **argv)
{
    processOptions(argc, argv);

    if (argc > 1) {
        printUsage();
        THROW(Error::LTFSDM_GENERAL_ERROR);
    }

    try {
        connect();
    } catch (const std::exception& e) {
        MSG(LTFSDMC0026E);
        return;
    }

    LTFSDmProtocol::LTFSDmRetrieveRequest *retrievereq =
            commCommand.mutable_retrieverequest();
    retrievereq->set_key(key);

    try {
        commCommand.send();
    } catch (const std::exception& e) {
        MSG(LTFSDMC0027E);
        THROW(Error::LTFSDM_GENERAL_ERROR);
    }

    try {
        commCommand.recv();
    } catch (const std::exception& e) {
        MSG(LTFSDMC0028E);
        THROW(Error::LTFSDM_GENERAL_ERROR);
    }

    const LTFSDmProtocol::LTFSDmRetrieveResp retrieveresp =
            commCommand.retrieveresp();

    switch (retrieveresp.error()) {
        case Error::LTFSDM_DRIVE_BUSY:
            MSG(LTFSDMC0095I);
            break;
        case Error::LTFSDM_OK:
            break;
        default:
            MSG(LTFSDMC0094E);
            THROW(Error::LTFSDM_GENERAL_ERROR);
    }
}
