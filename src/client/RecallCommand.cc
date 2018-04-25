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
#include <blkid/blkid.h>

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
#include "src/common/FileSystems.h"
#include "src/common/Configuration.h"

#include "src/connector/Connector.h"

#include "LTFSDMCommand.h"
#include "RecallCommand.h"

/** @page ltfsdm_recall ltfsdm recall
    The ltfsdm recall command is used to selectively recall one or more files from tape.

    <tt>@LTFSDMC0002I</tt>

    parameters | description
    ---|---
    -r | to recall files to resident state, without specifying this option files get recalled to premigrated state
    -n \<request number\> | attach to an ongoing recall request to see its progress
    \<file name\> | a set of file names of files to be recalled
    -f \<file list\> | a file name containing a list of files to be recalled

    Example:

    @verbatim
    [root@visp sdir]# time -p find dir.* -type f |ltfsdm recall -f -
    --- sending completed within 470 seconds ---
                   resident  transferred  premigrated     migrated       failed
    [00:08:07]            0            0        19089       980911            0
    [00:08:17]            0            0        45460       954540            0
    [00:08:27]            0            0        71614       928386            0
    [00:08:37]            0            0        97978       902022            0
    [00:08:47]            0            0       124256       875744            0
    [00:08:57]            0            0       150820       849180            0
    [00:09:07]            0            0       177126       822874            0
    [00:09:17]            0            0       203667       796333            0
    [00:09:27]            0            0       230053       769947            0
    [00:09:37]            0            0       256636       743364            0
    [00:09:47]            0            0       282795       717205            0
    [00:09:57]            0            0       309180       690820            0
    [00:10:07]            0            0       335801       664199            0
    [00:10:17]            0            0       362352       637648            0
    [00:10:27]            0            0       388731       611269            0
    [00:10:37]            0            0       414920       585080            0
    [00:10:47]            0            0       441328       558672            0
    [00:10:57]            0            0       467469       532531            0
    [00:11:07]            0            0       494076       505924            0
    [00:11:17]            0            0       520329       479671            0
    [00:11:27]            0            0       546638       453362            0
    [00:11:37]            0            0       572859       427141            0
    [00:11:47]            0            0       599138       400862            0
    [00:11:57]            0            0       625164       374836            0
    [00:12:07]            0            0       651295       348705            0
    [00:12:17]            0            0       677979       322021            0
    [00:12:27]            0            0       704257       295743            0
    [00:12:37]            0            0       730872       269128            0
    [00:12:47]            0            0       757410       242590            0
    [00:12:57]            0            0       784288       215712            0
    [00:13:07]            0            0       810809       189191            0
    [00:13:17]            0            0       837339       162661            0
    [00:13:27]            0            0       863603       136397            0
    [00:13:37]            0            0       889898       110102            0
    [00:13:47]            0            0       916118        83882            0
    [00:13:57]            0            0       942182        57818            0
    [00:14:07]            0            0       968286        31714            0
    [00:14:17]            0            0       994684         5316            0
    [00:14:27]            0            0      1000000            0            0
    [00:14:40]            0            0      1000000            0            0
    @endverbatim

    The corresponding class is @ref RecallCommand.

    @page recall_processing recall processing

    The following is a summary of the recall command processing:

    @code
    RecallCommand::doCommand
        LTFSDMCommand::processOptions
        LTFSDMCommand::checkOptions
        if filename arguments
            create stream containing file names
        LTFSDMCommand::isValidRegularFile
        RecallCommand::talkToBackend
            create selrecrequest message
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
    three steps that are performed are the following

    - general selective recall information is sent to the backend
    - the file names are send to the backend
    - the progress or results are queried
 */

void RecallCommand::printUsage()
{
    INFO(LTFSDMC0002I);
}

void RecallCommand::talkToBackend(std::stringstream *parmList)

{
    try {
        connect();
    } catch (const std::exception& e) {
        MSG(LTFSDMC0026E);
        return;
    }

    LTFSDmProtocol::LTFSDmSelRecRequest *recreq =
            commCommand.mutable_selrecrequest();

    recreq->set_key(key);
    recreq->set_reqnumber(requestNumber);
    recreq->set_pid(getpid());

    if (recToResident == true)
        recreq->set_state(FsObj::RESIDENT);
    else
        recreq->set_state(FsObj::PREMIGRATED);

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

    const LTFSDmProtocol::LTFSDmSelRecRequestResp recreqresp =
            commCommand.selrecrequestresp();

    switch (recreqresp.error()) {
        case static_cast<long>(Error::OK):
            if (getpid() != recreqresp.pid()) {
                MSG(LTFSDMC0036E);
                TRACE(Trace::error, getpid(), recreqresp.pid());
                THROW(Error::GENERAL_ERROR);
            }
            if (requestNumber != recreqresp.reqnumber()) {
                MSG(LTFSDMC0037E);
                TRACE(Trace::error, requestNumber, recreqresp.reqnumber());
                THROW(Error::GENERAL_ERROR);
            }
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

void RecallCommand::doCommand(int argc, char **argv)
{
    std::stringstream parmList;

    if (argc == 1) {
        INFO(LTFSDMC0018E);
        THROW(Error::COMMAND_FAILED);

    }

    processOptions(argc, argv);

    try {
        checkOptions(argc, argv);
    } catch (const std::exception& e) {
        printUsage();
        THROW(Error::COMMAND_FAILED);
    }

    TRACE(Trace::normal, argc, optind);
    traceParms();

    try {
        if (!fileList.compare("")) {
            for (int i = optind; i < argc; i++) {
                parmList << argv[i] << std::endl;
            }
        }

        isValidRegularFile();

        talkToBackend(&parmList);
    } catch (const std::exception& e) {
        MSG(LTFSDMC0025E);
    }

    if ((recToResident == true && resident == 0)
            || (recToResident == false && premigrated == 0)) {
        if (not_all_exist == true || failed > 0)
            THROW(Error::COMMAND_FAILED);
    } else if (not_all_exist == true || failed > 0) {
        THROW(Error::COMMAND_PARTIALLY_FAILED);
    }
}
