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
#include "PoolRemoveCommand.h"

/** @page ltfsdm_pool_remove ltfsdm pool remove
    The ltfsdm pool remove command removes a cartridge from a tape storage pool.

    <tt>@LTFSDMC0078I</tt>

    parameters | description
    ---|---
    -P \<pool name\> | pool name to which a cartridge should be removed
    -t \<tape id\> | id of a cartridge to be removed

    Example:

    @verbatim
    [root@visp ~]# ltfsdm pool remove -P 'large pool' -t DV1478L6
    Tape DV1478L6 successfully removed from pool "large pool".
    @endverbatim

    The corresponding class is @ref PoolRemoveCommand.
 */

void PoolRemoveCommand::printUsage()
{
    INFO(LTFSDMC0078I);
}

void PoolRemoveCommand::doCommand(int argc, char **argv)
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

    LTFSDmProtocol::LTFSDmPoolRemoveRequest *poolremovereq =
            commCommand.mutable_poolremoverequest();
    poolremovereq->set_key(key);
    poolremovereq->set_poolname(poolNames);

    for (std::string tapeid : tapeList)
        poolremovereq->add_tapeid(tapeid);

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
                INFO(LTFSDMC0086I, tapeid, poolNames);
                break;
            case static_cast<long>(Error::POOL_NOT_EXISTS):
                MSG(LTFSDMX0025E, poolNames);
                break;
            case static_cast<long>(Error::TAPE_NOT_EXISTS):
                MSG(LTFSDMC0084E, tapeid);
                break;
            case static_cast<long>(Error::TAPE_NOT_EXISTS_IN_POOL):
                MSG(LTFSDMX0022E, tapeid, poolNames);
                break;
            default:
                MSG(LTFSDMC0085E, tapeid, poolNames);
        }
    }
}
