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
#include <sstream>
#include <set>
#include <vector>
#include <list>
#include <thread>
#include <exception>
#include <memory>

#include "src/common/errors.h"
#include "src/common/LTFSDMException.h"
#include "src/common/util.h"
#include "src/common/Message.h"
#include "src/common/Trace.h"

#include "src/communication/ltfsdm.pb.h"
#include "src/communication/LTFSDmComm.h"

#include "LTFSDMCommand.h"
#include "StartCommand.h"
#include "StopCommand.h"
#include "AddCommand.h"
#include "MigrateCommand.h"
#include "RecallCommand.h"
#include "HelpCommand.h"
#include "InfoCommand.h"
#include "InfoRequestsCommand.h"
#include "InfoFilesCommand.h"
#include "InfoJobsCommand.h"
#include "InfoFsCommand.h"
#include "StatusCommand.h"
#include "InfoDrivesCommand.h"
#include "InfoTapesCommand.h"
#include "PoolCommand.h"
#include "PoolCreateCommand.h"
#include "PoolDeleteCommand.h"
#include "PoolAddCommand.h"
#include "PoolRemoveCommand.h"
#include "InfoPoolsCommand.h"
#include "RetrieveCommand.h"
#include "VersionCommand.h"


/**
 @page client_code Client Code

 # Class Hierarchy

 For each of the commands there exists a separate class that is derived
 from the @ref LTFSDMCommand class. The class name of a command
 consists of the string "Command" that is appended to the command name.

 @dot
 digraph client {
     fontname=default;
     fontsize=11;
     rankdir=RL;
     node [shape=record, fontsize=11, fillcolor=white, style=filled];
     open_ltfs_command [fontname="courier bold", fontcolor=dodgerblue4, width=2, label="LTFSDMCommand" URL="@ref LTFSDMCommand"];
     command [fontname="courier", fontcolor=black, label="ltfsdm X Y &rarr; class XYCommand"];
     command -> open_ltfs_command;
 }
 @enddot

 E.g. for the @ref ltfsdm_migrate "ltfsdm migrate" and for the @ref ltfsdm_recall "ltfsdm recall"
 commands there exist @ref MigrateCommand and @ref RecallCommand classes:

 @dot
 digraph client {
     fontname=default;
     fontsize=11;
     rankdir=RL;
     node [shape=record, fontname="courier bold", fontcolor=dodgerblue4, fontsize=11, fillcolor=white, style=filled, width=2];
     open_ltfs_command [label="LTFSDMCommand" URL="@ref LTFSDMCommand"];
     migrate_command [label="MigrateCommand" URL="@ref MigrateCommand"];
     recall_command [label="RecallCommand" URL="@ref RecallCommand"];
     other_commands [fontname="courier", fontcolor=black, label="..."];
     migrate_command -> open_ltfs_command;
     recall_command -> open_ltfs_command;
     other_commands -> open_ltfs_command;
 }
 @enddot

 Any new command should follow this rule of creating a corresponding class name.

 The actual processing of a command happens within virtual
 the LTFSDMCommand::doCommand method. Any command needs to implement
 this method even there is no need to talk to the backend.

 # Command evaluation

 LTFS Data Management is started, stopped, and operated using the [ltfsdm executable](@ref md_src_1_commands).

 For all commands the first and for the info and pool commands
 also the second argument of the ltfsdm executable is evaluated.

 @dot
 digraph command_processing {
    fontname=default;
    fontsize=11;
    color=white;
    fillcolor=white;
    node [shape=record, fontname=courier, fontsize=11, style=filled, color=white, fillcolor=white];
    subgraph cluster_command_line {
        ranktype=max;
        ltfsdm [color=white, label="ltfsdm"];
        argv_1 [color=white, label="argv[1]"];
        argv_2 [color=white, label="argv[2]"];
        argv_x [color=white, label="..."];
    }
    compare [ label = "…Command.compare(argv[1])" ];
    info_compare [ label = "Info…Command.compare(argv[2])" ];
    pool_compare [ label = "Pool…Command.compare(argv[2])" ];
    ltfsdm -> compare [color=transparent];      // necessary only for correct order
    argv_1 -> info_compare [color=transparent]; // necessary only for correct order
    argv_1 -> compare [];
    argv_2 -> info_compare [];
    argv_2 -> pool_compare [];
    argv_x -> pool_compare [color=transparent]; // necessary only for correct order
    object [ label = "ltfsdmCommand = std::unique_ptr\<LTFSDMCommand\>(new \[Info\|Pool\|\]…Command)" ];
    compare -> object [];
    info_compare -> object [];
    pool_compare -> object [];
    do_command [ label = "LTFSDMCommand-\>doCommand(argc, argv)" ];
    object -> do_command [];
 }
 @enddot

 */

void signalHandler(sigset_t set)

{
    int sig;

    while ( true) {
        if (sigwait(&set, &sig)) {
            if (sig == SIGUSR1)
                return;
            continue;
        }
        exitClient = true;
        break;
    }
}

int main(int argc, char *argv[])

{
    std::unique_ptr<LTFSDMCommand> ltfsdmCommand(nullptr);
    std::string command;
    int rc = static_cast<int>(Error::OK);
    sigset_t set;

    sigemptyset(&set);
    sigaddset(&set, SIGQUIT);
    sigaddset(&set, SIGINT);
    sigaddset(&set, SIGTERM);
    sigaddset(&set, SIGPIPE);
    sigaddset(&set, SIGUSR1);
    pthread_sigmask(SIG_BLOCK, &set, NULL);

    std::thread sigHandler(signalHandler, set);

    try {
        LTFSDM::init();
    } catch (const std::exception& e) {
        rc = Const::COMMAND_FAILED;
        goto end;
    }

    traceObject.setTrclevel(Trace::none);

    if (argc < 2) {
        ltfsdmCommand = std::unique_ptr<LTFSDMCommand>(new HelpCommand);
        ltfsdmCommand->doCommand(argc, argv);
        rc = Const::COMMAND_FAILED;
        goto end;
    }

    command = argv[1];

    TRACE(Trace::normal, argc, command.c_str());

    if (StartCommand().compare(command)) {
        ltfsdmCommand = std::unique_ptr<LTFSDMCommand>(new StartCommand);
    } else if (StopCommand().compare(command)) {
        ltfsdmCommand = std::unique_ptr<LTFSDMCommand>(new StopCommand);
    } else if (AddCommand().compare(command)) {
        ltfsdmCommand = std::unique_ptr<LTFSDMCommand>(new AddCommand);
    } else if (MigrateCommand().compare(command)) {
        ltfsdmCommand = std::unique_ptr<LTFSDMCommand>(new MigrateCommand);
    } else if (RecallCommand().compare(command)) {
        ltfsdmCommand = std::unique_ptr<LTFSDMCommand>(new RecallCommand);
    } else if (HelpCommand().compare(command)) {
        ltfsdmCommand = std::unique_ptr<LTFSDMCommand>(new HelpCommand);
    } else if (StatusCommand().compare(command)) {
        ltfsdmCommand = std::unique_ptr<LTFSDMCommand>(new StatusCommand);
    } else if (RetrieveCommand().compare(command)) {
        ltfsdmCommand = std::unique_ptr<LTFSDMCommand>(new RetrieveCommand);
    } else if (VersionCommand().compare(command)) {
        ltfsdmCommand = std::unique_ptr<LTFSDMCommand>(new VersionCommand);
    } else if (InfoCommand().compare(command)) {
        if (argc < 3) {
            MSG(LTFSDMC0011E);
            InfoCommand().printUsage();
            rc = Const::COMMAND_FAILED;
            goto end;
        }
        argc--;
        argv++;
        command = argv[1];
        TRACE(Trace::normal, command.c_str());
        if (InfoRequestsCommand().compare(command)) {
            ltfsdmCommand = std::unique_ptr<LTFSDMCommand>(
                    new InfoRequestsCommand);
        } else if (InfoJobsCommand().compare(command)) {
            ltfsdmCommand = std::unique_ptr<LTFSDMCommand>(new InfoJobsCommand);
        } else if (InfoFilesCommand().compare(command)) {
            ltfsdmCommand = std::unique_ptr<LTFSDMCommand>(
                    new InfoFilesCommand);
        } else if (InfoFsCommand().compare(command)) {
            ltfsdmCommand = std::unique_ptr<LTFSDMCommand>(new InfoFsCommand);
        } else if (InfoDrivesCommand().compare(command)) {
            ltfsdmCommand = std::unique_ptr<LTFSDMCommand>(
                    new InfoDrivesCommand);
        } else if (InfoTapesCommand().compare(command)) {
            ltfsdmCommand = std::unique_ptr<LTFSDMCommand>(
                    new InfoTapesCommand);
        } else if (InfoPoolsCommand().compare(command)) {
            ltfsdmCommand = std::unique_ptr<LTFSDMCommand>(
                    new InfoPoolsCommand);
        } else {
            MSG(LTFSDMC0012E, command.c_str());
            ltfsdmCommand = std::unique_ptr<LTFSDMCommand>(new HelpCommand);
            rc = Const::COMMAND_FAILED;
            goto end;
        }
    } else if (PoolCommand().compare(command)) {
        if (argc < 3) {
            MSG(LTFSDMC0074E);
            PoolCommand().printUsage();
            rc = Const::COMMAND_FAILED;
            goto end;
        }
        argc--;
        argv++;
        command = argv[1];
        TRACE(Trace::normal, command.c_str());
        if (PoolCreateCommand().compare(command)) {
            ltfsdmCommand = std::unique_ptr<LTFSDMCommand>(
                    new PoolCreateCommand);
        } else if (PoolDeleteCommand().compare(command)) {
            ltfsdmCommand = std::unique_ptr<LTFSDMCommand>(
                    new PoolDeleteCommand);
        } else if (PoolAddCommand().compare(command)) {
            ltfsdmCommand = std::unique_ptr<LTFSDMCommand>(new PoolAddCommand);
        } else if (PoolRemoveCommand().compare(command)) {
            ltfsdmCommand = std::unique_ptr<LTFSDMCommand>(
                    new PoolRemoveCommand);
        } else {
            MSG(LTFSDMC0012E, command.c_str());
            ltfsdmCommand = std::unique_ptr<LTFSDMCommand>(new HelpCommand);
            rc = Const::COMMAND_FAILED;
            goto end;
        }
    } else {
        MSG(LTFSDMC0005E, command.c_str());
        ltfsdmCommand = std::unique_ptr<LTFSDMCommand>(new HelpCommand);
        rc = Const::COMMAND_FAILED;
        goto end;
    }

    TRACE(Trace::normal, ltfsdmCommand->getCommand());

    argc--;
    argv++;

    if (argc > 1) {
        TRACE(Trace::normal, argc, argv[1]);
    }

    try {
        //! [call_do_command]
        ltfsdmCommand->doCommand(argc, argv);
    } catch (const LTFSDMException& e) {
        switch (e.getError()) {
            case Error::OK:
                break;
            case Error::COMMAND_PARTIALLY_FAILED:
                rc = Const::COMMAND_PARTIALLY_FAILED;
                break;
            case Error::COMMAND_FAILED:
                rc = Const::COMMAND_FAILED;
                break;
            default:
                rc = Const::COMMAND_FAILED;
        }
    } catch (const std::exception& e) {
        rc = 2;
    }

    end:

    kill(getpid(), SIGUSR1);
    sigHandler.join();

    return (int) rc;
}
