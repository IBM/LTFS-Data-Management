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

    The corresponding class is @ref PoolDeleteCommand.
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
