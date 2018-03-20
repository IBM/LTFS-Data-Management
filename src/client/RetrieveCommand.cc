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
#include "RetrieveCommand.h"

/** @page ltfsdm_retrieve ltfsdm retrieve
    The ltfsdm retrieve command synchronizes the status of drives and cartridges with Spectrum Archive  LE.

    <tt>@LTFSDMC0093I</tt>

    parameters | description
    ---|---
    - | -

    Example:

    @verbatim
    [root@visp ~]# ltfsdm retrieve
    [root@visp ~]#
    @endverbatim

    The corresponding class is @ref RetrieveCommand.
 */

void RetrieveCommand::printUsage()
{
    INFO(LTFSDMC0093I);
}

void RetrieveCommand::doCommand(int argc, char **argv)
{
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

    LTFSDmProtocol::LTFSDmRetrieveRequest *retrievereq =
            commCommand.mutable_retrieverequest();
    retrievereq->set_key(key);

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

    const LTFSDmProtocol::LTFSDmRetrieveResp retrieveresp =
            commCommand.retrieveresp();

    switch (retrieveresp.error()) {
        case static_cast<long>(Error::DRIVE_BUSY):
            MSG(LTFSDMC0095I);
            break;
        case static_cast<long>(Error::OK):
            break;
        default:
            MSG(LTFSDMC0094E);
            THROW(Error::GENERAL_ERROR);
    }
}
