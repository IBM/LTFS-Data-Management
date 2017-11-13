#include <sys/resource.h>

#include <string>
#include <sstream>
#include <set>
#include <vector>
#include <list>
#include <thread>
#include <exception>
#include <memory>

#include "src/common/errors/errors.h"
#include "src/common/exception/OpenLTFSException.h"
#include "src/common/util/util.h"
#include "src/common/messages/Message.h"
#include "src/common/tracing/Trace.h"

#include "src/common/comm/ltfsdm.pb.h"
#include "src/common/comm/LTFSDmComm.h"

#include "OpenLTFSCommand.h"
#include "StartCommand.h"
#include "StopCommand.h"
#include "AddCommand.h"
#include "MigrationCommand.h"
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

void signalHandler(sigset_t set)

{
    int sig;

    while ( true) {
        if (sigwait(&set, &sig)) {
            if ( sig == SIGUSR1 )
                return;
            continue;
        }
        exitClient = true;
        break;
    }
}

int main(int argc, char *argv[])

{
    std::unique_ptr<OpenLTFSCommand> openLTFSCommand(nullptr);
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
        rc = static_cast<int>(Error::GENERAL_ERROR);
        goto end;
    }

    traceObject.setTrclevel(Trace::none);

    if (argc < 2) {
        openLTFSCommand = std::unique_ptr<OpenLTFSCommand>(new HelpCommand);
        openLTFSCommand->doCommand(argc, argv);
        rc = static_cast<int>(Error::GENERAL_ERROR);
        goto end;
    }

    command = argv[1];

    TRACE(Trace::normal, argc, command.c_str());

    if (StartCommand().compare(command)) {
        openLTFSCommand = std::unique_ptr<OpenLTFSCommand>(new StartCommand);
    } else if (StopCommand().compare(command)) {
        openLTFSCommand = std::unique_ptr<OpenLTFSCommand>(new StopCommand);
    } else if (AddCommand().compare(command)) {
        openLTFSCommand = std::unique_ptr<OpenLTFSCommand>(new AddCommand);
    } else if (MigrationCommand().compare(command)) {
        openLTFSCommand =
                std::unique_ptr<OpenLTFSCommand>(new MigrationCommand);
    } else if (RecallCommand().compare(command)) {
        openLTFSCommand = std::unique_ptr<OpenLTFSCommand>(new RecallCommand);
    } else if (HelpCommand().compare(command)) {
        openLTFSCommand = std::unique_ptr<OpenLTFSCommand>(new HelpCommand);
    } else if (StatusCommand().compare(command)) {
        openLTFSCommand = std::unique_ptr<OpenLTFSCommand>(new StatusCommand);
    } else if (RetrieveCommand().compare(command)) {
        openLTFSCommand = std::unique_ptr<OpenLTFSCommand>(new RetrieveCommand);
    } else if (VersionCommand().compare(command)) {
        openLTFSCommand = std::unique_ptr<OpenLTFSCommand>(new VersionCommand);
    } else if (InfoCommand().compare(command)) {
        if (argc < 3) {
            MSG(LTFSDMC0011E);
            InfoCommand().printUsage();
            rc = static_cast<int>(Error::GENERAL_ERROR);
            goto end;
        }
        argc--;
        argv++;
        command = argv[1];
        TRACE(Trace::normal, command.c_str());
        if (InfoRequestsCommand().compare(command)) {
            openLTFSCommand =
                    std::unique_ptr<OpenLTFSCommand>(new InfoRequestsCommand);
        } else if (InfoJobsCommand().compare(command)) {
            openLTFSCommand =
                    std::unique_ptr<OpenLTFSCommand>(new InfoJobsCommand);
        } else if (InfoFilesCommand().compare(command)) {
            openLTFSCommand =
                    std::unique_ptr<OpenLTFSCommand>(new InfoFilesCommand);
        } else if (InfoFsCommand().compare(command)) {
            openLTFSCommand =
                    std::unique_ptr<OpenLTFSCommand>(new InfoFsCommand);
        } else if (InfoDrivesCommand().compare(command)) {
            openLTFSCommand =
                    std::unique_ptr<OpenLTFSCommand>(new InfoDrivesCommand);
        } else if (InfoTapesCommand().compare(command)) {
            openLTFSCommand =
                    std::unique_ptr<OpenLTFSCommand>(new InfoTapesCommand);
        } else if (InfoPoolsCommand().compare(command)) {
            openLTFSCommand =
                    std::unique_ptr<OpenLTFSCommand>(new InfoPoolsCommand);
        } else {
            MSG(LTFSDMC0012E, command.c_str());
            openLTFSCommand = std::unique_ptr<OpenLTFSCommand>(new HelpCommand);
            rc = static_cast<int>(Error::GENERAL_ERROR);
            goto end;
        }
    } else if (PoolCommand().compare(command)) {
        if (argc < 3) {
            MSG(LTFSDMC0074E);
            PoolCommand().printUsage();
            rc = static_cast<int>(Error::GENERAL_ERROR);
            goto end;
        }
        argc--;
        argv++;
        command = argv[1];
        TRACE(Trace::normal, command.c_str());
        if (PoolCreateCommand().compare(command)) {
            openLTFSCommand =
                    std::unique_ptr<OpenLTFSCommand>(new PoolCreateCommand);
        } else if (PoolDeleteCommand().compare(command)) {
            openLTFSCommand =
                    std::unique_ptr<OpenLTFSCommand>(new PoolDeleteCommand);
        } else if (PoolAddCommand().compare(command)) {
            openLTFSCommand =
                    std::unique_ptr<OpenLTFSCommand>(new PoolAddCommand);
        } else if (PoolRemoveCommand().compare(command)) {
            openLTFSCommand =
                    std::unique_ptr<OpenLTFSCommand>(new PoolRemoveCommand);
        } else {
            MSG(LTFSDMC0012E, command.c_str());
            openLTFSCommand = std::unique_ptr<OpenLTFSCommand>(new HelpCommand);
            rc = static_cast<int>(Error::GENERAL_ERROR);
            goto end;
        }
    } else {
        MSG(LTFSDMC0005E, command.c_str());
        openLTFSCommand = std::unique_ptr<OpenLTFSCommand>(new HelpCommand);
        rc = static_cast<int>(Error::GENERAL_ERROR);
        goto end;
    }

    TRACE(Trace::normal, openLTFSCommand->getCommand());

    argc--;
    argv++;

    if (argc > 1) {
        TRACE(Trace::normal, argc, argv[1]);
    }

    try {
        openLTFSCommand->doCommand(argc, argv);
    } catch (const OpenLTFSException& e) {
        rc = static_cast<int>(e.getError());
    } catch (const std::exception& e) {
    }

    end:

    kill(getpid(), SIGUSR1);
    sigHandler.join();

    return (int) rc;
}
