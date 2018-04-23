/*******************************************************************************
 * Copyright 2018 IBM Corp. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *  https://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 *******************************************************************************/
#include <fcntl.h>
#include <sys/file.h>
#include <sys/resource.h>

#include <string>
#include <set>
#include <vector>
#include <list>
#include <sstream>
#include <exception>

#include "src/common/errors.h"
#include "src/common/LTFSDMException.h"
#include "src/common/util.h"
#include "src/common/Message.h"
#include "src/common/Trace.h"

#include "src/communication/ltfsdm.pb.h"
#include "src/communication/LTFSDmComm.h"

#include "LTFSDMCommand.h"
#include "PoolAddCommand.h"

/** @page ltfsdm_pool_add ltfsdm pool add
    The ltfsdm pool add command adds a cartridge to a tape storage pool.

    <tt>@LTFSDMC0077I</tt>

    parameters | description
    ---|---
    -F | format a cartridge before add to a tape storage pool
    -C | check a cartridge before add to a tape storage pool
    -P \<pool name\> | pool name to which a cartridge should be added
    -t \<tape id\> | id of a cartridge to be added

    Example:

    @verbatim
    [root@visp ~]# ltfsdm pool add -P 'large pool' -t DV1478L6
    Tape DV1478L6 successfully added to pool "large pool".
    @endverbatim

    The corresponding class is @ref PoolAddCommand.
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

    if (format && check) {
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
    pooladdreq->set_format(format);
    pooladdreq->set_check(check);
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
            case static_cast<long>(Error::ALREADY_FORMATTED):
                MSG(LTFSDMX0083E, tapeid);
                break;
            case static_cast<long>(Error::NOT_FORMATTED):
                MSG(LTFSDMX0084E, tapeid);
                break;
            case static_cast<long>(Error::TAPE_NOT_WRITABLE):
                MSG(LTFSDMX0085E, tapeid);
                break;
            case static_cast<long>(Error::UNKNOWN_FORMAT_STATUS):
                MSG(LTFSDMX0086E, tapeid);
                break;
            case static_cast<long>(Error::CONFIG_POOL_NOT_EXISTS):
                MSG(LTFSDMX0025E, poolNames);
                return;
            default:
                MSG(LTFSDMC0085E, tapeid, poolNames);
        }
    }
}
