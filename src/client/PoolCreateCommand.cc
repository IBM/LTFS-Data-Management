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

#include "src/common/errors/errors.h"
#include "src/common/exception/OpenLTFSException.h"
#include "src/common/util/util.h"
#include "src/common/messages/Message.h"
#include "src/common/tracing/Trace.h"

#include "src/common/comm/ltfsdm.pb.h"
#include "src/common/comm/LTFSDmComm.h"

#include "OpenLTFSCommand.h"
#include "PoolCreateCommand.h"

/** @page ltfsdm_pool_create ltfsdm pool create
    The ltfsdm pool create command created a tape storage pool.

    <tt>@LTFSDMC0075I</tt>

    parameters | description
    ---|---
    -P \<pool name\> | pool name of the tape storage pool to be created

    Example:

    @verbatim
    [root@visp ~]# ltfsdm pool create -P newpool
    Pool "newpool" successfully created.
    @endverbatim

    The responsible class is @ref PoolCreateCommand.
 */

void PoolCreateCommand::printUsage()
{
    INFO(LTFSDMC0075I);
}

void PoolCreateCommand::doCommand(int argc, char **argv)
{
    if (argc <= 2) {
        printUsage();
        THROW(Error::GENERAL_ERROR);
    }

    processOptions(argc, argv);

    if (argc != optind) {
        printUsage();
        THROW(Error::GENERAL_ERROR);
    }

    if (std::count(poolNames.begin(), poolNames.end(), 10) > 0) {
        MSG(LTFSDMC0091E);
        THROW(Error::GENERAL_ERROR);
    }

    if (std::count(poolNames.begin(), poolNames.end(), 44) > 0) {
        MSG(LTFSDMC0092E);
        THROW(Error::GENERAL_ERROR);
    }

    try {
        connect();
    } catch (const std::exception& e) {
        MSG(LTFSDMC0026E);
        THROW(Error::GENERAL_ERROR);
    }

    LTFSDmProtocol::LTFSDmPoolCreateRequest *poolcreatereq =
            commCommand.mutable_poolcreaterequest();
    poolcreatereq->set_key(key);
    poolcreatereq->set_poolname(poolNames);

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

    const LTFSDmProtocol::LTFSDmPoolResp poolresp = commCommand.poolresp();

    switch (poolresp.response()) {
        case static_cast<long>(Error::OK):
            INFO(LTFSDMC0079I, poolNames);
            break;
        case static_cast<long>(Error::POOL_EXISTS):
            MSG(LTFSDMX0023E, poolNames);
            break;
        default:
            MSG(LTFSDMC0080E, poolNames);
    }
}
