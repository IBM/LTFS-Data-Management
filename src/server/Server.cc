#include "ServerIncludes.h"

std::atomic<bool> Server::terminate;
std::atomic<bool> Server::forcedTerminate;
std::atomic<bool> Server::finishTerminate;
std::mutex Server::termmtx;
std::condition_variable Server::termcond;

void Server::signalHandler(sigset_t set, long key)

{
    int sig;
    int requestNumber = ++globalReqNumber;

    while ( true) {
        if (sigwait(&set, &sig))
            continue;

        if (sig == SIGPIPE) {
            MSG(LTFSDMS0048E);
            continue;
        }

        MSG(LTFSDMS0085I);

        MSG(LTFSDMS0049I, sig);

        LTFSDmCommClient commCommand(Const::CLIENT_SOCKET_FILE);

        try {
            commCommand.connect();
        } catch (const std::exception& e) {
            TRACE(Trace::error, e.what());
            goto end;
        }

        TRACE(Trace::always, requestNumber);
        bool finished = false;

        do {
            LTFSDmProtocol::LTFSDmStopRequest *stopreq =
                    commCommand.mutable_stoprequest();
            stopreq->set_key(key);
            stopreq->set_reqnumber(requestNumber);
            stopreq->set_forced(false);
            stopreq->set_finish(true);

            try {
                commCommand.send();
            } catch (const std::exception& e) {
                TRACE(Trace::error, e.what());
                goto end;
            }

            try {
                commCommand.recv();
            } catch (const std::exception& e) {
                TRACE(Trace::error, e.what());
                goto end;
            }

            const LTFSDmProtocol::LTFSDmStopResp stopresp =
                    commCommand.stopresp();

            finished = stopresp.success();

            if (!finished) {
                sleep(1);
            } else {
                break;
            }
        } while (true);

        if (sig == SIGUSR1)
            goto end;
    }

    end:
    MSG(LTFSDMS0086I);
}

void Server::lockServer()

{
    int lockfd;

    if ((lockfd = open(Const::SERVER_LOCK_FILE.c_str(), O_RDWR | O_CREAT, 0600))
            == -1) {
        MSG(LTFSDMS0001E);
        TRACE(Trace::error, Const::SERVER_LOCK_FILE, errno);
        THROW(Const::UNSET, errno);
    }

    if (flock(lockfd, LOCK_EX | LOCK_NB) == -1) {
        TRACE(Trace::error, errno);
        if ( errno == EWOULDBLOCK) {
            MSG(LTFSDMS0002I);
            THROW(Const::UNSET, errno);
        } else {
            MSG(LTFSDMS0001E);
            TRACE(Trace::error, errno);
            THROW(Const::UNSET, errno);
        }
    }
}

void Server::writeKey()

{
    std::ofstream keyFile;

    keyFile.exceptions(std::ofstream::failbit | std::ofstream::badbit);

    try {
        keyFile.open(Const::KEY_FILE, std::fstream::out | std::fstream::trunc);
    } catch (const std::exception& e) {
        TRACE(Trace::error, e.what());
        MSG(LTFSDMS0003E);
        THROW(Const::UNSET);
    }

    srandom(time(NULL));
    key = random();
    keyFile << key << std::endl;

    keyFile.close();
}

void Server::initialize(bool dbUseMemory)

{
    if (setrlimit(RLIMIT_NOFILE, &Const::NOFILE_LIMIT) == -1) {
        MSG(LTFSDMS0046E);
        THROW(errno, errno);
    }

    if (setrlimit(RLIMIT_NPROC, &Const::NPROC_LIMIT) == -1) {
        MSG(LTFSDMS0046E);
        THROW(errno, errno);
    }

    lockServer();
    writeKey();

    unlink(Const::CLIENT_SOCKET_FILE.c_str());
    unlink(Const::RECALL_SOCKET_FILE.c_str());

    try {
        DB.cleanup();
        DB.open(dbUseMemory);
        DB.createTables();
    } catch (const std::exception& e) {
        TRACE(Trace::error, e.what());
        MSG(LTFSDMS0014E);
        THROW(Const::UNSET);
    }
}

void Server::daemonize()

{
    pid_t pid, sid;
    int dev_null;

    pid = fork();
    if (pid < 0) {
        exit(EXIT_FAILURE);
    }

    if (pid > 0) {
        THROW(Error::LTFSDM_OK);
    }

    sid = setsid();
    if (sid < 0) {
        MSG(LTFSDMS0012E);
        THROW(Const::UNSET, sid);
    }

    TRACE(Trace::always, getpid());

    messageObject.setLogType(Message::LOGFILE);

    /* redirect stdout to log file */
    if ((dev_null = open("/dev/null", O_RDWR)) == -1) {
        MSG(LTFSDMS0013E);
        THROW(errno, errno);
    }
    dup2(dev_null, STDIN_FILENO);
    dup2(dev_null, STDOUT_FILENO);
    dup2(dev_null, STDERR_FILENO);
    close(dev_null);
}

void Server::run(Connector *connector, sigset_t set)

{
    SubServer subs;
    Scheduler sched;
    Receiver recv;
    TransRecall trec;

    Server::terminate = false;
    Server::forcedTerminate = false;
    Server::finishTerminate = false;

    Scheduler::wqs = new ThreadPool<Migration::mig_info_t,
            std::shared_ptr<std::list<unsigned long>>>(&Migration::stub,
            Const::NUM_STUBBING_THREADS, "stub-wq");

    subs.enqueue("Scheduler", &Scheduler::run, &sched, key);
    subs.enqueue("Signal Handler", &Server::signalHandler, set, key);
    subs.enqueue("Receiver", &Receiver::run, &recv, key, connector);
    subs.enqueue("RecallD", &TransRecall::run, &trec, connector);

    subs.waitAllRemaining();

    MSG(LTFSDMS0087I);

    TRACE(Trace::always, (bool) Server::terminate,
            (bool) Server::forcedTerminate, (bool) Server::finishTerminate);

    for (std::shared_ptr<OpenLTFSDrive> drive : inventory->getDrives())
        drive->wqp->terminate();

    Scheduler::wqs->terminate();

    MSG(LTFSDMS0088I);
}
