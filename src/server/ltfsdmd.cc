#include "src/common/Version.h"
#include "ServerIncludes.h"

/**
    @page server_code

    # Invoking the backend

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
    -m | Store the Sqlite database in memory. By default it is stored in "/var/run" which usually is memory mapped.
    -d | Use a different trace level. See @ref tracing_system "tracing" for details of trace levels.



 */

int main(int argc, char **argv)

{
    Server ltfsdmd;
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

    sigemptyset(&set);
    sigaddset(&set, SIGQUIT);
    sigaddset(&set, SIGINT);
    sigaddset(&set, SIGTERM);
    sigaddset(&set, SIGPIPE);
    sigaddset(&set, SIGUSR1);
    pthread_sigmask(SIG_BLOCK, &set, NULL);

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
