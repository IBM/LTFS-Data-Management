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
#include <sys/resource.h>

#include <string>
#include <list>
#include <sstream>
#include <exception>

#include "src/common/errors.h"
#include "src/common/LTFSDMException.h"
#include "src/common/Message.h"
#include "src/common/Trace.h"

#include "src/communication/ltfsdm.pb.h"
#include "src/communication/LTFSDmComm.h"

#include "LTFSDMCommand.h"
#include "StatusCommand.h"

/** @page ltfsdm_status ltfsdm status
    The ltfsdm status command provides the status of the LTFS Data Management service.

    <tt>@LTFSDMC0030I</tt>

    parameters | description
    ---|---
    - | -

    Example:

    @verbatim
    [root@visp ~]# ltfsdm status
    LTFSDMC0032I(0068): The LTFS Data Management server process is operating with pid  13378.
    @endverbatim

    The corresponding class is @ref StatusCommand.
 */

void StatusCommand::printUsage()
{
    INFO(LTFSDMC0030I);
}

void StatusCommand::doCommand(int argc, char **argv)
{
    int pid;

    processOptions(argc, argv);

    if (argc > 1) {
        printUsage();
        THROW(Error::GENERAL_ERROR);
    }

    try {
        connect();
    } catch (const std::exception& e) {
        MSG(LTFSDMC0026E);
        return;
    }

    TRACE(Trace::normal, requestNumber);

    commCommand.Clear();

    LTFSDmProtocol::LTFSDmStatusRequest *statusreq =
            commCommand.mutable_statusrequest();
    statusreq->set_key(key);
    statusreq->set_reqnumber(requestNumber);

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

    const LTFSDmProtocol::LTFSDmStatusResp statusresp =
            commCommand.statusresp();

    if (statusresp.success() == true) {
        pid = statusresp.pid();
        MSG(LTFSDMC0032I, pid);
    } else {
        MSG(LTFSDMC0029E);
        THROW(Error::GENERAL_ERROR);
    }
}
