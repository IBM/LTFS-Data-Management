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
#include <sys/resource.h>

#include <string>
#include <sstream>
#include <list>
#include <exception>

#include "src/common/errors/errors.h"
#include "src/common/exception/LTFSDMException.h"
#include "src/common/messages/Message.h"
#include "src/common/tracing/Trace.h"

#include "src/common/comm/ltfsdm.pb.h"
#include "src/common/comm/LTFSDmComm.h"

#include "LTFSDMCommand.h"
#include "MigrateCommand.h"

/** @page ltfsdm_migrate ltfsdm migrate
    The ltfsdm migrate command is used to migrate one or more files to tape.

    <tt>@LTFSDMC0001I</tt>

    parameters | description
    ---|---
    -p | to premigrate files, without specifying this option files get migrated
    -P \<pool list: 'pool1,pool2,pool3'\> | a file can be migrated up to three different tape storage pools in parallel, at least one tape storage pool needs to be specified
    -n \<request number\> | attach to an ongoing migration request to see its progress
    \<file name\> | a set of file names of files to be migrated
    -f \<file list\> | a file name containing a list of files to be migrated

    Example:

    @verbatim
    [root@visp sdir]# find dir.* -type f |ltfsdm migrate -P pool1 -f -
    --- sending completed within 385 seconds ---
                   resident  transferred  premigrated     migrated       failed
    [00:06:44]       989202        10798            0            0            0
    [00:06:54]       980801        19199            0            0            0
    [00:07:06]       972900        27100            0            0            0
    [00:07:16]       949856        50144            0            0            0
    [00:07:26]       908483        91517            0            0            0
    [00:07:36]       867137       132863            0            0            0
    [00:07:46]       825946       174054            0            0            0
    [00:07:56]       784595       215405            0            0            0
    [00:08:06]       743378       256622            0            0            0
    [00:08:16]       702042       297958            0            0            0
    [00:08:26]       660813       339187            0            0            0
    [00:08:36]       619431       380569            0            0            0
    [00:08:46]       586392       413608            0            0            0
    [00:08:56]       545061       454939            0            0            0
    [00:09:06]       503781       496219            0            0            0
    [00:09:16]       462584       537416            0            0            0
    [00:09:26]       421563       578437            0            0            0
    [00:09:36]       380156       619844            0            0            0
    [00:09:46]       342861       657139            0            0            0
    [00:09:56]       321660       678340            0            0            0
    [00:10:06]       286385       713615            0            0            0
    [00:10:16]       245078       754922            0            0            0
    [00:10:26]       203848       796152            0            0            0
    [00:10:36]       181600       818400            0            0            0
    [00:10:46]       140372       859628            0            0            0
    [00:10:56]        99105       900895            0            0            0
    [00:11:06]        58311       941689            0            0            0
    [00:11:16]        17043       982957            0            0            0
    [00:12:21]            0      1000000            0            0            0
    [00:12:35]            0       941128            0        58872            0
    [00:12:45]            0       876747            0       123253            0
    [00:12:55]            0       812104            0       187896            0
    [00:13:05]            0       747845            0       252155            0
    [00:13:15]            0       684347            0       315653            0
    [00:13:25]            0       619911            0       380089            0
    [00:13:35]            0       555271            0       444729            0
    [00:13:45]            0       491031            0       508969            0
    [00:13:55]            0       426315            0       573685            0
    [00:14:05]            0       362974            0       637026            0
    [00:14:15]            0       299364            0       700636            0
    [00:14:25]            0       234782            0       765218            0
    [00:14:35]            0       170123            0       829877            0
    [00:14:45]            0       105773            0       894227            0
    [00:14:55]            0        41364            0       958636            0
    [00:15:09]            0            0            0      1000000            0
    [00:15:22]            0            0            0      1000000            0
    @endverbatim

    The corresponding class is @ref MigrateCommand.

    @page migrate_processing migrate processing

    The following is a summary of the migrate command processing:

    @code
    MigrateCommand::doCommand
        LTFSDMCommand::processOptions
        LTFSDMCommand::checkOptions
        if filename arguments
            create stream containing file names
        LTFSDMCommand::isValidRegularFile
        MigrateCommand::talkToBackend
            create migrequest message
            LTFSDmCommClient::send
            LTFSDmCommClient::recv
            evaluate response
            LTFSDMCommand::sendObjects
                while filenames to send
                    create sendobjecs message
                    LTFSDmCommClient::send
                    LTFSDmCommClient::recv
                    evaluate response
            LTFSDMCommand::queryResults
                do
                    create reqstatusrequest message
                    LTFSDmCommClient::send
                    LTFSDmCommClient::recv
                    print progress
                until not done
    @endcode

    After the option processing is done it is evaluated in which way the
    file names are provided to the command. There are three different
    possibilities:

    - the file names are provided as arguments to the command
    - a file list is provided containing the file names
    - a "-" is provided as file list name and the file names are provided by stdin

    If a file list is provided it is checked if it is a valid regular file.

    The further processing is performed by communicating with the backend. The
    three steps that are performed are the following:

    - general migration information is sent to the backend
    - the file names are send to the backend
    - the progress or results are queried

 */

void MigrateCommand::printUsage()
{
    INFO(LTFSDMC0001I);
}

void MigrateCommand::talkToBackend(std::stringstream *parmList)

{
    try {
        connect();
    } catch (const std::exception& e) {
        MSG(LTFSDMC0026E);
        return;
    }

    LTFSDmProtocol::LTFSDmMigRequest *migreq = commCommand.mutable_migrequest();

    migreq->set_key(key);
    migreq->set_reqnumber(requestNumber);
    migreq->set_pid(getpid());
    migreq->set_pools(poolNames);

    if (preMigrate == true)
        migreq->set_state(LTFSDmProtocol::LTFSDmMigRequest::PREMIGRATED);
    else
        migreq->set_state(LTFSDmProtocol::LTFSDmMigRequest::MIGRATED);

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

    const LTFSDmProtocol::LTFSDmMigRequestResp migreqresp =
            commCommand.migrequestresp();

    switch (migreqresp.error()) {
        case static_cast<long>(Error::OK):
            if (getpid() != migreqresp.pid()) {
                MSG(LTFSDMC0036E);
                TRACE(Trace::error, getpid(), migreqresp.pid());
                THROW(Error::GENERAL_ERROR);
            }
            if (requestNumber != migreqresp.reqnumber()) {
                MSG(LTFSDMC0037E);
                TRACE(Trace::error, requestNumber, migreqresp.reqnumber());
                THROW(Error::GENERAL_ERROR);
            }
            break;
        case static_cast<long>(Error::WRONG_POOLNUM):
            MSG(LTFSDMS0063E);
            THROW(Error::GENERAL_ERROR);
            break;
        case static_cast<long>(Error::NOT_ALL_POOLS_EXIST):
            MSG(LTFSDMS0064E);
            THROW(Error::GENERAL_ERROR);
            break;
        case static_cast<long>(Error::TERMINATING):
            MSG(LTFSDMC0101I);
            THROW(Error::GENERAL_ERROR);
            break;
        default:
            MSG(LTFSDMC0029E);
            THROW(Error::GENERAL_ERROR);
    }

    sendObjects(parmList);

    queryResults();
}

void MigrateCommand::doCommand(int argc, char **argv)
{
    std::stringstream parmList;

    if (argc == 1) {
        INFO(LTFSDMC0018E);
        THROW(Error::GENERAL_ERROR);
    }

    processOptions(argc, argv);

    try {
        checkOptions(argc, argv);
    } catch (const std::exception& e) {
        printUsage();
        THROW(Error::GENERAL_ERROR);
    }

    TRACE(Trace::normal, argc, optind);
    traceParms();

    if (!fileList.compare("")) {
        for (int i = optind; i < argc; i++) {
            parmList << argv[i] << std::endl;
        }
    }

    isValidRegularFile();

    talkToBackend(&parmList);
}
