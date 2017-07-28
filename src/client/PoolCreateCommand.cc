#include <fcntl.h>
#include <sys/file.h>
#include <sys/resource.h>

#include <string>
#include <set>
#include <vector>
#include <list>
#include <algorithm>
#include <sstream>
#include <exception>

#include "src/common/exception/OpenLTFSException.h"
#include "src/common/util/util.h"
#include "src/common/messages/Message.h"
#include "src/common/tracing/Trace.h"
#include "src/common/errors/errors.h"

#include "src/common/comm/ltfsdm.pb.h"
#include "src/common/comm/LTFSDmComm.h"

#include "OpenLTFSCommand.h"
#include "PoolCreateCommand.h"

void PoolCreateCommand::printUsage()
{
    INFO(LTFSDMC0075I);
}

void PoolCreateCommand::doCommand(int argc, char **argv)
{
    if (argc <= 2) {
        printUsage();
        throw(EXCEPTION(Error::LTFSDM_GENERAL_ERROR));
    }

    processOptions(argc, argv);

    if (argc != optind) {
        printUsage();
        throw(EXCEPTION(Error::LTFSDM_GENERAL_ERROR));
    }

    if (std::count(poolNames.begin(), poolNames.end(), 10) > 0) {
        MSG(LTFSDMC0091E);
        throw(EXCEPTION(Error::LTFSDM_GENERAL_ERROR));
    }

    if (std::count(poolNames.begin(), poolNames.end(), 44) > 0) {
        MSG(LTFSDMC0092E);
        throw(EXCEPTION(Error::LTFSDM_GENERAL_ERROR));
    }

    try {
        connect();
    } catch (const std::exception& e) {
        MSG(LTFSDMC0026E);
        throw(EXCEPTION(Error::LTFSDM_GENERAL_ERROR));
    }

    LTFSDmProtocol::LTFSDmPoolCreateRequest *poolcreatereq =
            commCommand.mutable_poolcreaterequest();
    poolcreatereq->set_key(key);
    poolcreatereq->set_poolname(poolNames);

    try {
        commCommand.send();
    } catch (const std::exception& e) {
        MSG(LTFSDMC0027E);
        throw(EXCEPTION(Error::LTFSDM_GENERAL_ERROR));
    }

    try {
        commCommand.recv();
    } catch (const std::exception& e) {
        MSG(LTFSDMC0028E);
        throw(EXCEPTION(Error::LTFSDM_GENERAL_ERROR));
    }

    const LTFSDmProtocol::LTFSDmPoolResp poolresp = commCommand.poolresp();

    switch (poolresp.response()) {
        case Error::LTFSDM_OK:
            INFO(LTFSDMC0079I, poolNames);
            break;
        case Error::LTFSDM_POOL_EXISTS:
            MSG(LTFSDMX0023E, poolNames);
            break;
        default:
            MSG(LTFSDMC0080E, poolNames);
    }
}
