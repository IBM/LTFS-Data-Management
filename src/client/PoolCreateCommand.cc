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
#include <algorithm>
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
#include "PoolCreateCommand.h"

/** @page ltfsdm_pool_create ltfsdm pool create
    The ltfsdm pool create command creates a tape storage pool.

    <tt>@LTFSDMC0075I</tt>

    parameters | description
    ---|---
    -P \<pool name\> | pool name of the tape storage pool to be created

    Example:

    @verbatim
    [root@visp ~]# ltfsdm pool create -P newpool
    Pool "newpool" successfully created.
    @endverbatim

    The corresponding class is @ref PoolCreateCommand.
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
