#include "src/common/Version.h"
#include "ServerIncludes.h"

/**
    @page server_code Server Code

    # The backend

    ## The ltfsdmd command

    The backend usually us started by the @ref ltfsdm_start "ltfsdm start"
    command. However it is possible to start the backend directly by using
    the

    @verbatim
    ltfsdmd [-f] [-m] [-d <debug level>]
    @endverbatim

    command.

    The option arguments have the following meaning:

    option | meaning
    :---:|---
    -f | Start the backend in foreground. Messages will be printed out to stdout.
    -m | Store the SQLite database in memory. By default it is stored in "/var/run" which usually is memory mapped.
    -d | Use a different trace level. See @ref tracing_system "tracing" for details of trace levels.

    ## Server components

    Main task of the backend is the processing of migration and recall
    requests and to perform the resource management of cartridges and
    tape drives. To optimally use the resources for migration and recall
    requests requests need to be queued and schedules when a required
    resource is ready to be used. The following is a high level sequence
    how such requests get processed:

    - a request and corresponding jobs are added to the internal queues
    - the scheduler is looking for a free drive and tape resource
    - if there exists a corresponding free resource the request gets scheduled
    - the jobs can be processed on that tape

    The term request and job here is used in the following way:

    - A request is an operation started by a single command. If a user
      likes to migrate a lot of files by a single command there will
      be a single migration request. The scheduler only is considering
      requests to be scheduled.
    - A job is a single operation that is performed on a cartridge. For
      migration a job is related to a file to be migrated. If there is a
      migration request to migrate 1000 files to a single tape 1000 jobs
      will be created.

    Requests as well as some takes can and should happen concurrently.
    E.g. there are two drives available and some files get recalled
    form a single tape another tape can be used in parallel e.g. for
    migration. If a request is being processed on a tape it should be
    possible to add further requests and jobs to the internal queues
    in parallel.

    For concurrency threads are created. Some of them run all the
    time - some others are started on request. For the latter case
    thread pools are available where threads automatically terminate
    if not longer used.

    The following threads and thread pools are available after starting
    the backend:


    ## The startup sequence

    During the startup initialization happens and threads are started for
    further processing. The following gives an overview:

    @dot
    digraph startup {
        compound=true;
        fontname="fixed";
        fontsize=11;
        labeljust=l;
        node [shape=record, width=2, fontname="fixed", fontsize=11, fillcolor=white, style=filled];
        ltfsdmd [fontname="fixed bold", fontcolor=dodgerblue4, label="main", URL="@ref ltfsdmd.cc::main"];
        create_server [ label="create Server object: ltfsdmd"];
        option_processing[label="option processing"];
        setup_signal_handling[label="setup signal handling"];
        subgraph cluster_init {
            fontname="fixed bold";
            fontcolor=dodgerblue4;
            label="LTFSDM::init";
            URL="@ref LTFSDM::init"
            init [ label="make temporary directory|<it> initialize tracing|initialize messaging"];
        }
        subgraph cluster_init_server {
            fontname="fixed bold";
            fontcolor=dodgerblue4;
            label="initialize server: ltfsdmd.initialize";
            URL="@ref Server::initialize"
            serv_init [ label="setup resource limits|lock server|write key|initialize database"];
        }
        daemonize [ fontname="fixed bold", fontcolor=dodgerblue4, label="daemonize: ltfsdmd.daemonize", URL="@ref Server::daemonize" ];
        subgraph cluster_run_server {
            fontname="fixed bold";
            fontcolor=dodgerblue4;
            label="run server: ltfsdmd.run";
            URL="@ref Server::run"
            read_config [fontname="fixed bold", fontcolor=dodgerblue4, label="read configuration", URL="@ref Configuration::read"];
            ínventorize [fontname="fixed bold", fontcolor=dodgerblue4, label="inventorize", URL="@ref OpenLTFSInventory"];
            connector [fontname="fixed bold", fontcolor=dodgerblue4, label="connector", URL="@ref Connector"];
            stub_thread_pool [ label="create stubbing thread pool"];
            start_scheduler [fontname="fixed bold", fontcolor=dodgerblue4, label="start scheduler", URL="@ref Scheduler::run"];
            start_signal_handler [fontname="fixed bold", fontcolor=dodgerblue4, label="start signal handler", URL="@ref Receiver::run"];
            start_receiver [fontname="fixed bold", fontcolor=dodgerblue4, label="start receiver", URL="@ref Server::signalHandler"];
            start_recall_listener [fontname="fixed bold", fontcolor=dodgerblue4, label="start recall listener", URL="@ref TransRecall::run"];
            read_config -> ínventorize -> connector -> stub_thread_pool -> start_scheduler -> start_signal_handler -> start_receiver -> start_recall_listener [];
        }
        ltfsdmd -> create_server -> option_processing -> setup_signal_handling [];
        setup_signal_handling -> init [lhead=cluster_init,minlen=2];
        init -> serv_init [ltail=cluster_init,lhead=cluster_init_server,minlen=2];
        serv_init -> daemonize [ltail=cluster_init_server,minlen=2];
        daemonize -> read_config [lhead=cluster_run_server,minlen=2];

    }
    @enddot

    The items within the chart have the following purpose:

    item | description
    ---|---
    main|main entry point
    create Server object: ltfsdmd|see: @snippet server/ltfsdmd.cc define server object
    option processing|process options for the ltfsdmd command:  @snippet server/ltfsdmd.cc option processing
    setup signal handling|setup the signals for signal handling: @snippet server/ltfsdmd.cc setup signals
    LTFSDM::init |initialize common items that are not server specific: @snippet common/util/util.cc init
    LTFSDM::init -> make temporary directory|create temporary directory /var/run/ltfsdm, see: mkTmpDir()
    LTFSDM::init -> initialize tracing| see: Message::init
    LTFSDM::init -> initialize messaging| see: Trace::init
    initialize server: ltfsdmd.initialize | see: Server::initialize
    initialize server: ltfsdmd.initialize -> setup system limits| set the application specific resource limits: @snippet server/Server.cc set resource limits
    initialize server: ltfsdmd.initialize -> lock server| see: Server::lockServer()
    initialize server: ltfsdmd.initialize -> write key file| see: Server::writeKey()
    initialize server: ltfsdmd.initialize -> initialize database| initialize the internal SQLite database: @snippet server/Server.cc init db
    daemonize: ltfsdmd.daemonize | detaching the server process, see: Server::daemonize
    run server: ltfsdmd.run | performing the remaining initialization and starting the main threads, see Server::init
    run server: ltfsdmd.run -> read configuration | read the configuration file: @snippet server/Server.cc read the configuration file
    run server: ltfsdmd.run -> inventorize | import information from LTFS about cartridges and drives: @snippet server/Server.cc inventorize
    run server: ltfsdmd.run -> connector | create a connector object: @snippet server/Server.cc connector
    run server: ltfsdmd.run -> create stubbing thread pool | create a thread pool for the stubbing operations: @snippet server/Server.cc thread pool for stubbing
    run server: ltfsdmd.run -> start scheduler | see: Scheduler::run
    run server: ltfsdmd.run -> start signal handler| see: Server::signalHandler
    run server: ltfsdmd.run -> start receiver| see: Receiver::run
    run server: ltfsdmd.run -> start recall listener | thread listening for transparent recall requests, see TransRecall::run

 */

int main(int argc, char **argv)

{
    //! [define server object]
    Server ltfsdmd;
    //! [define server object]
    int err = static_cast<int>(Error::OK);
    bool detach = true;
    int opt;
    sigset_t set;
    bool dbUseMemory = false;
    Trace::traceLevel tl = Trace::error;

    opterr = 0;

    if (chdir("/") == -1) {
        MSG(LTFSDMS0092E, errno);
        goto end;
    }

    //! [option processing]
    while ((opt = getopt(argc, argv, "fmd:")) != -1) {
        switch (opt) {
            case 'f':
                detach = false;
                break;
            case 'm':
                dbUseMemory = true;
                break;
            case 'd':
                try {
                    tl = (Trace::traceLevel) std::stoi(optarg);
                } catch (const std::exception& e) {
                    TRACE(Trace::error, e.what());
                    tl = Trace::error;
                }
                break;
            default:
                std::cerr << messages[LTFSDMC0013E] << std::endl;
                err = static_cast<int>(Error::GENERAL_ERROR);
                goto end;
        }
    }
    //! [option processing]

    //! [setup signals]
    sigemptyset(&set);
    sigaddset(&set, SIGQUIT);
    sigaddset(&set, SIGINT);
    sigaddset(&set, SIGTERM);
    sigaddset(&set, SIGPIPE);
    sigaddset(&set, SIGUSR1);
    pthread_sigmask(SIG_BLOCK, &set, NULL);
    //! [setup signals]

    try {
        LTFSDM::init();
    } catch (const std::exception& e) {
        err = static_cast<int>(Error::GENERAL_ERROR);
        goto end;
    }

    traceObject.setTrclevel(tl);
    TRACE(Trace::always, getpid());
    MSG(LTFSDMX0029I, OPENLTFS_VERSION);

    try {
        ltfsdmd.initialize(dbUseMemory);

        if (detach)
            ltfsdmd.daemonize();

        MSG(LTFSDMX0029I, OPENLTFS_VERSION);

        ltfsdmd.run(set);
    } catch (const OpenLTFSException& e) {
        if (e.getError() != Error::OK) {
            TRACE(Trace::error, e.what());
            err = static_cast<int>(Error::GENERAL_ERROR);
        }
    } catch (const std::exception& e) {
        TRACE(Trace::error, e.what());
        err = static_cast<int>(Error::GENERAL_ERROR);
    }

    end: return (int) err;
}
