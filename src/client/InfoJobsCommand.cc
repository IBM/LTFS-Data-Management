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
#include <blkid/blkid.h>

#include <unistd.h>
#include <string>
#include <list>
#include <map>
#include <set>
#include <sstream>
#include <exception>

#include "src/common/errors.h"
#include "src/common/LTFSDMException.h"
#include "src/common/Message.h"
#include "src/common/Trace.h"
#include "src/common/FileSystems.h"
#include "src/common/Configuration.h"

#include "src/communication/ltfsdm.pb.h"
#include "src/communication/LTFSDmComm.h"

#include "src/connector/Connector.h"

#include "LTFSDMCommand.h"
#include "InfoJobsCommand.h"

/** @page ltfsdm_info_jobs ltfsdm info jobs
    The ltfsdm info jobs command lists all jobs that are currently processed by the backend.

    <tt>@LTFSDMC0059I</tt>

    parameters | description
    ---|---
    -n \<request number\> | restrict the jobs to be displayed to a certain request

    Example:

    @verbatim
    [root@visp ~]# ltfsdm info jobs -n 2
    operation            state                request number       tape pool            tape id              size                 file name
    migration            transferring         2                    pool1                DV1462L6             1073741824           /mnt/lxfs/test1/file.0
    migration            transferring         2                    pool1                DV1462L6             1073741824           /mnt/lxfs/test1/file.1
    migration            transferring         2                    pool1                DV1462L6             1073741824           /mnt/lxfs/test1/file.2
    migration            transferring         2                    pool1                DV1462L6             1073741824           /mnt/lxfs/test1/file.3
    migration            transferring         2                    pool1                DV1462L6             1073741824           /mnt/lxfs/test1/file.4
    migration            transferring         2                    pool1                DV1462L6             1073741824           /mnt/lxfs/test1/file.5
    migration            transferring         2                    pool1                DV1462L6             1073741824           /mnt/lxfs/test1/file.6
    migration            transferring         2                    pool1                DV1462L6             1073741824           /mnt/lxfs/test1/file.7
    migration            transferring         2                    pool1                DV1462L6             1073741824           /mnt/lxfs/test1/file.8
    migration            transferring         2                    pool1                DV1462L6             1073741824           /mnt/lxfs/test1/file.9
    @endverbatim

    The corresponding class is @ref InfoJobsCommand.
 */

void InfoJobsCommand::printUsage()
{
    INFO(LTFSDMC0059I);
}

void InfoJobsCommand::doCommand(int argc, char **argv)
{
    long reqOfInterest;

    processOptions(argc, argv);

    TRACE(Trace::normal, *argv, argc, optind);

    if (argc != optind) {
        printUsage();
        THROW(Error::GENERAL_ERROR);
    } else if (requestNumber < Const::UNSET) {
        printUsage();
        THROW(Error::GENERAL_ERROR);
    }

    reqOfInterest = requestNumber;

    try {
        connect();
    } catch (const std::exception& e) {
        MSG(LTFSDMC0026E);
        return;
    }

    LTFSDmProtocol::LTFSDmInfoJobsRequest *infojobs =
            commCommand.mutable_infojobsrequest();

    infojobs->set_key(key);
    infojobs->set_reqnumber(reqOfInterest);

    try {
        commCommand.send();
    } catch (const std::exception& e) {
        MSG(LTFSDMC0027E);
        THROW(Error::GENERAL_ERROR);
    }

    INFO(LTFSDMC0062I);
    int recnum;

    do {
        try {
            commCommand.recv();
        } catch (const std::exception& e) {
            MSG(LTFSDMC0028E);
            THROW(Error::GENERAL_ERROR);
        }

        const LTFSDmProtocol::LTFSDmInfoJobsResp infojobsresp =
                commCommand.infojobsresp();
        std::string operation = infojobsresp.operation();
        std::string filename = infojobsresp.filename();
        recnum = infojobsresp.reqnumber();
        std::string pool = infojobsresp.pool();
        unsigned long size = infojobsresp.filesize();
        std::string tapeid = infojobsresp.tapeid();
        std::string state = FsObj::migStateStr(infojobsresp.state());
        if (recnum != Const::UNSET)
            INFO(LTFSDMC0063I, operation, state, recnum, pool, tapeid, size,
                    filename);

    } while (!exitClient && recnum != Const::UNSET);

    return;
}
