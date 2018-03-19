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
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/resource.h>
#include <unistd.h>
#include <libmount/libmount.h>
#include <blkid/blkid.h>

#include <string>
#include <map>
#include <set>
#include <vector>
#include <list>
#include <sstream>
#include <exception>

#include "src/common/errors/errors.h"
#include "src/common/exception/LTFSDMException.h"
#include "src/common/util/util.h"
#include "src/common/util/FileSystems.h"
#include "src/common/messages/Message.h"
#include "src/common/tracing/Trace.h"
#include "src/common/configuration/Configuration.h"

#include "src/common/comm/ltfsdm.pb.h"
#include "src/common/comm/LTFSDmComm.h"

#include "src/connector/Connector.h"

#include "LTFSDMCommand.h"
#include "InfoFsCommand.h"

/** @page ltfsdm_info_fs ltfsdm info fs
    The ltfsdm info fs command lists all file systems that are managed with LTFS Data Management:

    <tt>@LTFSDMC0056I</tt>

    parameters | description
    ---|---
    - | -

    @bug Does not work currently. This command uses old code to determine the filesystems being managed.

    Example:

    @verbatim
    @endverbatim

    The corresponding class is @ref InfoFsCommand.
 */

void InfoFsCommand::printUsage()
{
    INFO(LTFSDMC0056I);
}

void InfoFsCommand::talkToBackend(std::stringstream *parmList)

{
}

void InfoFsCommand::doCommand(int argc, char **argv)
{
    Connector connector(false);
    Configuration conf;

    processOptions(argc, argv);

    try {
        conf.read();
    } catch (const std::exception& e) {
        MSG(LTFSDMX0038E);
        TRACE(Trace::error, e.what());
        THROW(Error::GENERAL_ERROR);
    }

    if (argc > 1) {
        printUsage();
        THROW(Error::GENERAL_ERROR);
    }

    INFO(LTFSDMC0105I);

    try {
        FileSystems fss;
        for (std::string mountpt : conf.getFss()) {
            try {
                FileSystems::fsinfo fs = conf.getFs(mountpt);
                INFO(LTFSDMC0057I, fs.source, fs.target, fs.fstype, fs.options);
            } catch (const std::exception& e) {
                INFO(LTFSDMC0058E, mountpt);
            }
        }
    } catch (const std::exception& e) {
        TRACE(Trace::error, e.what());
        THROW(Error::GENERAL_ERROR);
    }
}
