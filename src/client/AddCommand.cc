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
#include <libmount/libmount.h>
#include <blkid/blkid.h>

#include <string>
#include <set>
#include <vector>
#include <list>
#include <sstream>
#include <exception>

#include "src/common/errors.h"
#include "src/common/LTFSDMException.h"
#include "src/common/util.h"
#include "src/common/FileSystems.h"
#include "src/common/Message.h"
#include "src/common/Trace.h"

#include "src/communication/ltfsdm.pb.h"
#include "src/communication/LTFSDmComm.h"

#include "LTFSDMCommand.h"
#include "AddCommand.h"

/** @page ltfsdm_add ltfsdm add
    The ltfsdm add command is used to add file system management
    with LTFS Data Management to a particular file system.

    <tt>@LTFSDMC0052I</tt>

    parameters | description
    ---|---
    \<mount point\> | path to the mount point of the file system to be managed

    Example:
    @verbatim
    [root@visp ~]# ltfsdm add /mnt/xfs
    @endverbatim

    The corresponding class is @ref AddCommand.
 */

void AddCommand::printUsage()
{
    INFO(LTFSDMC0052I);
}

void AddCommand::doCommand(int argc, char **argv)
{
    char *pathName = NULL;
    std::string managedFs;

    if (argc == 1) {
        printUsage();
        THROW(Error::GENERAL_ERROR);
    }

    processOptions(argc, argv);

    pathName = canonicalize_file_name(argv[optind]);

    if (pathName == NULL) {
        MSG(LTFSDMC0053E);
        THROW(Error::GENERAL_ERROR);
    }

    managedFs = pathName;
    free(pathName);

    try {
        FileSystems fss;
        FileSystems::fsinfo fs = fss.getByTarget(managedFs);
    } catch (const std::exception& e) {
        MSG(LTFSDMC0053E);
        THROW(Error::GENERAL_ERROR);
    }

    if (argc != optind + 1) {
        printUsage();
        THROW(Error::GENERAL_ERROR);
    }

    try {
        connect();
    } catch (const std::exception& e) {
        MSG(LTFSDMC0026E);
        THROW(Error::GENERAL_ERROR);
    }

    TRACE(Trace::normal, requestNumber);

    LTFSDmProtocol::LTFSDmAddRequest *addreq = commCommand.mutable_addrequest();
    addreq->set_key(key);
    addreq->set_reqnumber(requestNumber);
    addreq->set_managedfs(managedFs);

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

    const LTFSDmProtocol::LTFSDmAddResp addresp = commCommand.addresp();

    if (addresp.response() == LTFSDmProtocol::LTFSDmAddResp::FAILED) {
        MSG(LTFSDMC0055E, managedFs);
        THROW(Error::GENERAL_ERROR);
    } else if (addresp.response()
            == LTFSDmProtocol::LTFSDmAddResp::ALREADY_ADDED) {
        MSG(LTFSDMC0054I, managedFs);
    }
}
