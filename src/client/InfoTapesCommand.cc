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

#include <unistd.h>
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
#include "InfoTapesCommand.h"

/** @page ltfsdm_info_tapes ltfsdm info tapes
    The ltfsdm info tapes command lists all available cartridges and corresponding space information.

    <tt>@LTFSDMC0065I</tt>

    parameters | description
    ---|---
    - | -

    Example:

    @verbatim
    [root@visp ~]# ltfsdm info tapes
    id              slot            total           remaining       reclaimable     in progress     status          pool            state
                                    capacity (GiB)  capacity (GiB)  estimated (GiB) (GiB)
    DV1462L6        4112            0               0               0               0               not mounted yet n/a             not mounted
    DV1463L6        4096            0               0               0               0               not mounted yet n/a             not mounted
    DV1464L6        257             2242            2222            1               0               writable        pool1           mounted
    DV1465L6        4102            0               0               0               0               not mounted yet n/a             not mounted
    DV1466L6        4115            0               0               0               0               not mounted yet n/a             not mounted
    DV1467L6        4110            2242            2072            178             0               writable        pool2           not mounted
    DV1468L6        256             2242            2238            2               0               writable        pool3           mounted
    @endverbatim

    The corresponding class is @ref InfoTapesCommand.
 */

void InfoTapesCommand::printUsage()
{
    INFO(LTFSDMC0065I);
}

void InfoTapesCommand::doCommand(int argc, char **argv)
{
    processOptions(argc, argv);

    TRACE(Trace::normal, *argv, argc, optind);

    if (argc != optind) {
        printUsage();
        THROW(Error::GENERAL_ERROR);
    }

    try {
        connect();
    } catch (const std::exception& e) {
        MSG(LTFSDMC0026E);
        return;
    }

    LTFSDmProtocol::LTFSDmInfoTapesRequest *infotapes =
            commCommand.mutable_infotapesrequest();

    infotapes->set_key(key);

    try {
        commCommand.send();
    } catch (const std::exception& e) {
        MSG(LTFSDMC0027E);
        THROW(Error::GENERAL_ERROR);
    }

    INFO(LTFSDMC0066I);

    std::string id;

    do {
        try {
            commCommand.recv();
        } catch (const std::exception& e) {
            MSG(LTFSDMC0028E);
            THROW(Error::GENERAL_ERROR);
        }

        const LTFSDmProtocol::LTFSDmInfoTapesResp infotapesresp =
                commCommand.infotapesresp();
        id = infotapesresp.id();
        unsigned long slot = infotapesresp.slot();
        unsigned long totalcap = infotapesresp.totalcap();
        unsigned long remaincap = infotapesresp.remaincap();
        unsigned long reclaimable = infotapesresp.reclaimable();
        std::string status = infotapesresp.status();
        std::replace(status.begin(), status.end(), '_', ' ');
        std::transform(status.begin(), status.end(), status.begin(), ::tolower);
        unsigned long inprogress = infotapesresp.inprogress();
        std::string pool = infotapesresp.pool();
        if (pool.compare("") == 0)
            pool = ltfsdm_messages[LTFSDMC0090I];
        std::string state = infotapesresp.state();
        if (id.compare("") != 0)
            INFO(LTFSDMC0067I, id, slot, totalcap, remaincap, reclaimable,
                    inprogress, status, pool, state);
    } while (!exitClient && id.compare("") != 0);

    return;
}
