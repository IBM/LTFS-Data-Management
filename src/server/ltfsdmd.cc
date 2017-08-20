#include "src/common/Version.h"
#include "ServerIncludes.h"

int main(int argc, char **argv)

{
    Server ltfsdmd;
    int err = Error::LTFSDM_OK;
    bool detach = true;
    int opt;
    sigset_t set;
    bool dbUseMemory = false;
    Trace::traceLevel tl = Trace::error;

    Connector *connector = NULL;

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
                err = Error::LTFSDM_GENERAL_ERROR;
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
        traceObject.setTrclevel(tl);
    } catch (const std::exception& e) {
        exit((int) Error::LTFSDM_GENERAL_ERROR);
    }

    try {
        LTFSDM::init();
    } catch (const std::exception& e) {
        TRACE(Trace::error, e.what());
        err = Error::LTFSDM_GENERAL_ERROR;
        goto end;
    }

    TRACE(Trace::always, getpid());

    try {
        ltfsdmd.initialize(dbUseMemory);

        if (detach)
            ltfsdmd.daemonize();

        MSG(LTFSDMX0029I, OPENLTFS_VERSION);

        inventory = new OpenLTFSInventory();
        connector = new Connector(true);
        ltfsdmd.run(connector, set);
    } catch (const OpenLTFSException& e) {
        if (e.getError() != Error::LTFSDM_OK) {
            TRACE(Trace::error, e.what());
            err = Const::UNSET;
        }
    } catch (const std::exception& e) {
        TRACE(Trace::error, e.what());
        err = Const::UNSET;
    }

    end: if (inventory)
        delete (inventory);
    if (connector)
        delete (connector);
    return (int) err;
}
