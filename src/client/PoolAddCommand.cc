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
#include "PoolAddCommand.h"

/** @page ltfsdm_pool_add ltfsdm pool add
    The ltfsdm pool add command adds a cartridge to a tape storage pool.

    <tt>@LTFSDMC0077I</tt>

    parameters | description
    ---|---
    -P \<pool name\> | pool name to which a cartridge should be added
    -t \<tape id\> | id of a cartridge to be added

    Example:

    @verbatim
    [root@visp ~]# ltfsdm pool add -P 'large pool' -t DV1478L6
    Tape DV1478L6 successfully added to pool "large pool".
    @endverbatim

    The responsible class is @ref PoolAddCommand.
 */

void PoolAddCommand::printUsage()
{
    INFO(LTFSDMC0077I);
}

void PoolAddCommand::doCommand(int argc, char **argv)
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

    LTFSDmProtocol::LTFSDmPoolAddRequest *pooladdreq =
            commCommand.mutable_pooladdrequest();
    pooladdreq->set_key(key);
    pooladdreq->set_poolname(poolNames);

    for (std::string tapeid : tapeList)
        pooladdreq->add_tapeid(tapeid);

    try {
        commCommand.send();
    } catch (const std::exception& e) {
        MSG(LTFSDMC0027E);
        THROW(Error::GENERAL_ERROR);
    }

    for (unsigned int i = 0; i < tapeList.size(); i++) {

        try {
            commCommand.recv();
        } catch (const std::exception& e) {
            MSG(LTFSDMC0028E);
            THROW(Error::GENERAL_ERROR);
        }

        const LTFSDmProtocol::LTFSDmPoolResp poolresp = commCommand.poolresp();

        std::string tapeid = poolresp.tapeid();

        switch (poolresp.response()) {
            case static_cast<long>(Error::OK):
                INFO(LTFSDMC0083I, tapeid, poolNames);
                break;
            case static_cast<long>(Error::POOL_NOT_EXISTS):
                MSG(LTFSDMX0025E, poolNames);
                break;
            case static_cast<long>(Error::TAPE_NOT_EXISTS):
                MSG(LTFSDMC0084E, tapeid);
                break;
            case static_cast<long>(Error::TAPE_EXISTS_IN_POOL):
                MSG(LTFSDMX0021E, tapeid);
                break;
            default:
                MSG(LTFSDMC0085E, tapeid, poolNames);
        }
    }
}
