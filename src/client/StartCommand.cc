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
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <limits.h>
#include <libgen.h>
#include <fcntl.h>
#include <sys/file.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/resource.h>
#include <errno.h>

#ifdef __APPLE__
#include <mach-o/dyld.h>
#endif

#include <string>
#include <sstream>
#include <list>
#include <exception>

#include "src/common/errors.h"
#include "src/common/LTFSDMException.h"
#include "src/common/Message.h"
#include "src/common/Trace.h"
#include "src/common/Const.h"

#include "src/communication/ltfsdm.pb.h"
#include "src/communication/LTFSDmComm.h"

#include "LTFSDMCommand.h"
#include "StartCommand.h"

/** @page ltfsdm_start ltfsdm start
    The ltfsdm start command starts the LTFS Data Management service.

    <tt>@LTFSDMC0006I</tt>

    parameters | description
    ---|---
    - | -

    Example:

    @verbatim
    [root@visp ~]# ltfsdm start
    LTFSDMC0099I(0073): Starting the LTFS Data Management backend service.
    LTFSDMX0029I(0062): LTFS Data Management version: 0.0.624-master.2017-11-09T10.57.51
    LTFSDMC0100I(0097): Connecting.
    LTFSDMC0097I(0141): The LTFS Data Management server process has been started with pid  13378.
    @endverbatim

    The corresponding class is @ref StartCommand.

    @page start_processing start processing

    The following is a summary of the start command processing:

    @code
    StartCommand::doCommand
       StartCommand::determineServerPath
       StartCommand::startServer
       StartCommand::waitForResponse
           while not connected and retry<10
               LTFSDmCommClient::connect
               sleep 1
           if retry == 10
               exit with failue
           create statusrequest message
           LTFSDmCommClient::send
           LTFSDmCommClient::recv
           evaluate response
    @endcode

    @dot
    digraph start_command {
        compound=true;
        fontname="courier";
        fontsize=11;
        labeljust=l;
        node [shape=record, width=2, fontname="courier", fontsize=11, fillcolor=white, style=filled];
        do_command [fontname="courier bold", fontcolor=dodgerblue4, label="StartCommand::doCommand", URL="@ref StartCommand::doCommand"];
        subgraph cluster_do_command {
            label="StartCommand::doCommand";
            determine_server_path [fontname="courier bold", fontcolor=dodgerblue4, label="StartCommand::determineServerPath", URL="@ref StartCommand::determineServerPath"];
            start_server [fontname="courier bold", fontcolor=dodgerblue4, label="StartCommand::startServer", URL="@ref StartCommand::startServer"];
            wait_for_response [fontname="courier bold", fontcolor=dodgerblue4, label="StartCommand::waitForResponse", URL="@ref StartCommand::waitForResponse"];
        }
        subgraph cluster_wait_for_response {
            label="StartCommand::waitForResponse";
            subgraph cluster_loop {
                label="while not connected and retry<10";
                connect [fontname="courier bold", fontcolor=dodgerblue4, label="LTFSDmCommClient::connect", URL="@ref LTFSDmCommClient::connect"];
                sleep [label="sleep 1"];
            }
            subgraph cluster_condition {
                label="if retry == 10";
                exit [label="exit with failue"];
            }
            create_message [label="create statusrequest message"];
            send [fontname="courier bold", fontcolor=dodgerblue4, label="LTFSDmCommClient::send", URL="@ref LTFSDmCommClient::send"];
            recv [fontname="courier bold", fontcolor=dodgerblue4, label="LTFSDmCommClient::recv", URL="@ref LTFSDmCommClient::recv"];
            response [label="evaluate response"];
        }
        do_command -> determine_server_path [lhead=cluster_do_command,minlen=2];
        determine_server_path -> start_server [];
        start_server -> wait_for_response [];
        wait_for_response -> connect [lhead=cluster_wait_for_response,minlen=2];
        connect -> sleep [];
        sleep -> exit [ltail=cluster_loop,lhead=cluster_condition,minlen=2];
        exit -> create_message [ltail=cluster_condition,minlen=2];
        create_message -> send [];
        send -> recv [];
        recv -> response [];
    }
    @enddot

    The start commands starts the LTFS Data Management server. To do so
    its path name needs to be detected. This is performed by the
    StartCommand::determineServerPath method. Since the client and the
    server path are the same it is just necessary to read
    the link to the executable of the current client process via procfs.

    The backend is started within the StartCommand::startServer method.
    It is started via popen system call.

    After the backend is started the status information is requested
    within the StartCommand::waitForResponse method. A connection is
    retried 10 times before giving up and reporting a failure.
 */

void StartCommand::printUsage()

{
    INFO(LTFSDMC0006I);
}

void StartCommand::determineServerPath()

{
    char exepath[PATH_MAX];

    TRACE(Trace::normal, Const::SERVER_COMMAND);

    memset(exepath, 0, PATH_MAX);
    if (readlink("/proc/self/exe", exepath, PATH_MAX - 1) == -1) {
        MSG(LTFSDMC0021E);
        THROW(Error::GENERAL_ERROR);
    }

    serverPath << dirname(exepath) << "/" << Const::SERVER_COMMAND;

    TRACE(Trace::normal, serverPath.str());
}

void StartCommand::startServer()

{
    struct stat statbuf;
    FILE *ltfsdmd = NULL;
    char line[Const::OUTPUT_LINE_SIZE];
    int ret;

    if (stat(serverPath.str().c_str(), &statbuf) == -1) {
        MSG(LTFSDMC0021E);
        TRACE(Trace::error, serverPath.str(), errno);
        THROW(Error::GENERAL_ERROR);
    }

    MSG(LTFSDMC0099I);

    ltfsdmd = popen(serverPath.str().c_str(), "r");

    if (!ltfsdmd) {
        MSG(LTFSDMC0022E);
        TRACE(Trace::error, errno);
        THROW(Error::GENERAL_ERROR);
    }

    while (fgets(line, sizeof(line), ltfsdmd)) {
        INFO(LTFSDMC0024I, line);
    }

    ret = pclose(ltfsdmd);

    if (!WIFEXITED(ret) || WEXITSTATUS(ret)) {
        MSG(LTFSDMC0022E);
        TRACE(Trace::error, ret, WIFEXITED(ret), WEXITSTATUS(ret));
        THROW(Error::GENERAL_ERROR);
    }
}

void StartCommand::waitForResponse()

{
    int pid;
    int retry = 0;
    bool success = false;
    int lockfd;

    MSG(LTFSDMC0100I);
    while (retry < Const::STARTUP_TIMEOUT) {
        try {
            connect();
            success = true;
            break;
        } catch (const std::exception& e) {
            INFO(LTFSDMC0103I);
            if ((lockfd = open(Const::SERVER_LOCK_FILE.c_str(),
                    O_RDWR | O_CREAT, 0600)) == -1) {
                INFO(LTFSDMC0104I);
                MSG(LTFSDMC0033E);
                return;
            }
            if (flock(lockfd, LOCK_EX | LOCK_NB) == 0) {
                INFO(LTFSDMC0104I);
                MSG(LTFSDMC0096E);
                return;
            }
            retry++;
            sleep(1);
        }
    }

    INFO(LTFSDMC0104I);

    if (success == false) {
        MSG(LTFSDMC0096E);
        return;
    }

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
        MSG(LTFSDMC0098E);
        THROW(Error::GENERAL_ERROR);
    }

    const LTFSDmProtocol::LTFSDmStatusResp statusresp =
            commCommand.statusresp();

    if (statusresp.success() == true) {
        pid = statusresp.pid();
        MSG(LTFSDMC0097I, pid);
    } else {
        MSG(LTFSDMC0098E);
        THROW(Error::GENERAL_ERROR);
    }

}

void StartCommand::doCommand(int argc, char **argv)

{
    if (argc > 1) {
        printUsage();
        THROW(Error::GENERAL_ERROR);
    }

    determineServerPath();
    startServer();
    waitForResponse();

}
