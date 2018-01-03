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
    resource is ready to be used. For the queuing information needs to
    be stored temporarily. Two SQLite tables are used for that purpose.
    A description of the two tables can be found at @subpage sqlite.

    The following is a high level sequence how such requests get processed:

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

    Some requests as well as some jobs can and should happen concurrently.
    E.g. there are two drives available and some files get recalled
    form a single tape another tape can be used in parallel e.g. for
    migration. If a request is being processed on a tape it should be
    possible to add further requests and jobs to the internal queues
    in parallel.

    For concurrency threads are created. Some of them run all the
    time - some others are started on request. For the latter case
    thread pools are available where threads automatically terminate
    if not longer used.

    The following threads are available after starting the backend:

    @verbatim
      Id   Target Id         Frame
      12   Thread 0x7f8f62c7a700 (LWP 16635) "ltfsdmd" 0x00007f8f65889a9b in recv () from /lib64/libpthread.so.0
      11   Thread 0x7f8f62479700 (LWP 16640) "Scheduler" 0x00007f8f65886945 in pthread_cond_wait@@GLIBC_2.3.2 () from /lib64/libpthread.so.0
      10   Thread 0x7f8f61c78700 (LWP 16641) "w:Scheduler" 0x00007f8f65883f57 in pthread_join () from /lib64/libpthread.so.0
      9    Thread 0x7f8f61477700 (LWP 16642) "SigHandler" 0x00007f8f6588a371 in sigwait () from /lib64/libpthread.so.0
      8    Thread 0x7f8f60c76700 (LWP 16643) "w:SigHandler" 0x00007f8f65883f57 in pthread_join () from /lib64/libpthread.so.0
      7    Thread 0x7f8f4bfff700 (LWP 16644) "Receiver" 0x00007f8f6588998d in accept () from /lib64/libpthread.so.0
      6    Thread 0x7f8f4b7fe700 (LWP 16645) "w:Receiver" 0x00007f8f65883f57 in pthread_join () from /lib64/libpthread.so.0
      5    Thread 0x7f8f4affd700 (LWP 16646) "RecallD" 0x00007f8f6588998d in accept () from /lib64/libpthread.so.0
      4    Thread 0x7f8f4a7fc700 (LWP 16647) "w:RecallD" 0x00007f8f65883f57 in pthread_join () from /lib64/libpthread.so.0
      3    Thread 0x7f8f49ffb700 (LWP 16648) "ltfsdmd.ofs" 0x00007f8f640f07fd in read () from /lib64/libc.so.6
      2    Thread 0x7f8f497fa700 (LWP 16662) "ltfsdmd.ofs" 0x00007f8f640f07fd in read () from /lib64/libc.so.6
    * 1    Thread 0x7f8f660e08c0 (LWP 16633) "ltfsdmd" 0x00007f8f65883f57 in pthread_join () from /lib64/libpthread.so.0
    @endverbatim

    These threads have the following purpose

    Id | function being executed | description
    :---:|---|---
    12 | - | communication with LTFS LE
    11 | Scheduler::run | schedules requests based on free resources
    10 | SubServer::waitThread | waits for Scheduler thread termination
    9 | Server::signalHandler | cares about signals
    8 | SubServer::waitThread | waits for signal handler thread termination
    7 | Receiver::run | listens for client messages
    6 | SubServer::waitThread | waits for Receiver thread termination
    5 | TransRecall::run | listens for transparent recall requests
    4 | SubServer::waitThread | waits for TransRecall thread termination
    3 | FuseFS::execute | started the Fuse connector process for a single file system
    2 | FuseFS::execute | started the Fuse connector process for another file system
    1 | ltfsdmd.cc:main() | main thread

    In this example two file systems are managed by LTFS Data Management. Therefore two
    Fuse threads exist. Thread pools are not visible after initial start since
    threads within a thread pool are created on request.

    The following thread pools exist:

    operation | object | function being executed | description
    ---|---|---|---
    message parsing | Receiver::run -> wqm | MessageParser::run | After the Receiver gets a new message this message is further processed by a new thread from this thread pool.
    premigration | LTFSDMDrive::wqp | Migration::preMigrate | For premigration there is one thread pool per drive since only a single request can be executed on a certain drive at a time.
    stubbing | Server::wqs | Migration::stub | There exist one thread pool for all stubbing operations (even from different requests).
    transparent recall | TransRecall::run -> wqr | TransRecall::addRequest | Adds a transparent recall request and waits for completion.

    Furthermore the scheduler is creating additional threads for each
    request being scheduled:

    function | description
    ---|---
    Migration::execRequest | schedules a migration request
    SelRecall::execRequest | schedules a selective recall request
    TransRecall::execRequest | schedules a transparent recall request

    For each of these threads there will be an additional waiter thread.

    Overall this leads to the following picture:

    @verbatim
    main()
    ltfsdmd.run
        communication with LTFS LE (1 thread)
        LTFSDMDrive::wqp(number of thread pools equal number of drives)
        FuseFS::execute (threads equal number of files systems)
        Server::wqs (1 thread pool)
        Scheduler::run (1 thread)
            Migration::execRequest (number of thread less or equal number of drives)
            SelRecall::execRequest (number of thread less or equal number of drives)
            TransRecall::execRequest (number of thread less or equal number of drives)
        Server::signalHandler (1 thread)
        Receiver::run (1 thread)
            Receiver::run -> wqm (1 thread pool)
        TransRecall::run (1 thread)
            TransRecall::run -> wqr (1 thread pool)
    @endverbatim

    To create threads there are two facilities created:
    - Threads normally created by the SubServer class. After the thread
      function finishes the thread is terminated. For each thread an
      additional waiter thread is created. See @subpage standard_thread.
    - For a better performance and less overhead a ThreadPool class
      exist. Threads will stay for another 10 seconds before termination
      after the thread function terminates. Within these 10 seconds it
      is possible to reuse them. See @subpage thread_pool.


    ## Backend Processing

    The following sections provides an overview about the backend processing of client requests:

    - @subpage receiver_and_message_processing
    - @subpage scheduler
    - @subpage migration
    - @subpage selective_recall

    Furthermore for transparent recalling the following sections are available:

    - @subpage transparent_recall

    ## The startup sequence

    During the startup initialization happens and threads are started for
    further processing.

    The configuration is read and the information
    about drives and cartridges are received from LTFS LE. The configuration
    provides information about the managed file systems and the tape
    storage pools. When the connector object is created these file systems
    will be managed. For the Fuse connector an overlay file system will
    be created for each managed file system. The creation of the drive and
    cartridge inventory in the following is called inventorize. During this
    operation premigration thread pools are created: one pool for each drive.
    A thread pool for the stubbing operation is setup. Thereafter the thread
    for scheduling, singnal handling, the receiver, and the listener for the
    transparent recall requests are started.

    The following gives an overview:

    @code
    main
        create Server object: ltfsdmd
        option processing
        setup signal handling
        LTFSDM::init
            make temporary directory
            initialize tracing
            initialize messaging
        initialize server: ltfsdmd.initialize
            setup system limits
            lock server
            write key file
            initialize database
        ltfsdmd.daemonize
        ltfsdmd.run
            read configuration
            inventorize
            connector
            create stubbing thread pool
            start scheduler
            start signal handler
            start receiver
            start recall listener
    @endcode

    @dot
    digraph startup {
        compound=true;
        fontname="fixed";
        fontsize=11;
        labeljust=l;
        rankdir=LR;
        node [shape=record, width=2, fontname="fixed", fontsize=11, fillcolor=white, style=filled];
        subgraph cluster_main {
            fontname="fixed bold";
            fontcolor=dodgerblue4;
            label="main";
            URL="@ref main";
            main [label="create Server\nobject: ltfsdmd|option\nprocessing|setup\nsignal\nhandling|<init> LTFSDM::init|<init_server> initialize server:\nltfsdmd.initialize|daemonize:\nltfsdmd.daemonize|<run> run server:\nltfsdmd.run"];
        }
        subgraph cluster_sub {
            label="";
            style=invis;
            subgraph cluster_init {
                style=solid;
                fontname="fixed bold";
                fontcolor=dodgerblue4;
                label="LTFSDM::init";
                URL="@ref LTFSDM::init"
                init [ label="make\ntemporary directory|<it> initialize tracing|<init_msg> initialize messaging"];
            }
            subgraph cluster_init_server {
                style=solid;
                fontname="fixed bold";
                fontcolor=dodgerblue4;
                label="initialize server:\nltfsdmd.initialize";
                URL="@ref Server::initialize"
                serv_init [ label="setup\nresource limits|lock server|write key|<init_db> initialize database"];
            }
            subgraph cluster_run_server {
                style=solid;
                fontname="fixed bold";
                fontcolor=dodgerblue4;
                label="run server:\nltfsdmd.run";
                URL="@ref Server::run"
                run [ label="read\nconfiguration|inventorize|connector|create stubbing\nthread pool|start\nscheduler|start\nsignal handler|start\nreceiver|<recalld> start\nrecall listener" ]
            }
            main:init -> init [lhead=cluster_init];
            main:init_server -> serv_init [lhead=cluster_init_server];
            main:run -> run [lhead=cluster_run_server];
        }
    }
    @enddot

    For each of the these items in the following there is a more
    detailed desciption. Most corresponding code is part of the
    ltfsdmd.cc and Server.cc files.

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
    MSG(LTFSDMX0029I, LTFSDM_VERSION);

    try {
        ltfsdmd.initialize(dbUseMemory);

        if (detach)
            ltfsdmd.daemonize();

        MSG(LTFSDMX0029I, LTFSDM_VERSION);

        ltfsdmd.run(set);
    } catch (const LTFSDMException& e) {
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
