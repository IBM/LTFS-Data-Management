#include <fcntl.h>
#include <sys/file.h>
#include <sys/resource.h>

#include <string>
#include <set>
#include <vector>
#include <list>
#include <sstream>
#include <exception>

#include "src/common/errors/errors.h"
#include "src/common/exception/LTFSDMException.h"
#include "src/common/util/util.h"
#include "src/common/messages/Message.h"
#include "src/common/tracing/Trace.h"

#include "src/common/comm/ltfsdm.pb.h"
#include "src/common/comm/LTFSDmComm.h"

#include "LTFSDMCommand.h"
#include "PoolDeleteCommand.h"

/** @page ltfsdm_pool_delete ltfsdm pool delete
    The ltfsdm pool delete command deletes a tape storage pool.

    <tt>@LTFSDMC0076I</tt>

    parameters | description
    ---|---
    -P \<pool name\> | pool name of the tape storage pool to be deleted

    Example:

    @verbatim
    [root@visp ~]# ltfsdm pool delete -P newpool
    Pool "newpool" successfully deleted.
    @endverbatim

    The responsible class is @ref PoolDeleteCommand.
 */

void PoolDeleteCommand::printUsage()
{
    INFO(LTFSDMC0076I);
}

void PoolDeleteCommand::doCommand(int argc, char **argv)
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

    try {
        connect();
    } catch (const std::exception& e) {
        MSG(LTFSDMC0026E);
        THROW(Error::GENERAL_ERROR);
    }

    LTFSDmProtocol::LTFSDmPoolDeleteRequest *pooldeletereq =
            commCommand.mutable_pooldeleterequest();
    pooldeletereq->set_key(key);
    pooldeletereq->set_poolname(poolNames);

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
            INFO(LTFSDMC0082I, poolNames);
            break;
        case static_cast<long>(Error::POOL_NOT_EXISTS):
            MSG(LTFSDMX0025E, poolNames);
            break;
        case static_cast<long>(Error::POOL_NOT_EMPTY):
            MSG(LTFSDMX0024E, poolNames);
            break;
        default:
            MSG(LTFSDMC0081E, poolNames);
    }

}
