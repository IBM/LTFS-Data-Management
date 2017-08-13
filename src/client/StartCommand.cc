#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <limits.h>
#include <libgen.h>
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

#include "src/common/exception/OpenLTFSException.h"
#include "src/common/messages/Message.h"
#include "src/common/tracing/Trace.h"
#include "src/common/errors/errors.h"
#include "src/common/const/Const.h"

#include "src/common/comm/ltfsdm.pb.h"
#include "src/common/comm/LTFSDmComm.h"

#include "OpenLTFSCommand.h"
#include "StartCommand.h"

void StartCommand::printUsage()

{
    INFO(LTFSDMC0006I);
}

void StartCommand::determineServerPath()

{
    char exepath[PATH_MAX];

    TRACE(Trace::normal, Const::SERVER_COMMAND);

#ifdef __linux__
    char *exelnk = (char*) "/proc/self/exe";

    if (readlink(exelnk, exepath, PATH_MAX) == -1) {
        MSG(LTFSDMC0021E);
        THROW(Error::LTFSDM_GENERAL_ERROR);
    }
#elif __APPLE__
    uint32_t size = PATH_MAX;
    if ( _NSGetExecutablePath(exepath, &size) != 0 ) {
        MSG(LTFSDMC0021E);
        TRACE(Trace::error, errno);
        THROW(Error::LTFSDM_GENERAL_ERROR);
    }
#else
#error "unsupported platform"
#endif

    serverPath << dirname(exepath) << "/" << Const::SERVER_COMMAND;

    TRACE(Trace::normal, serverPath.str());
}

void StartCommand::startServer()

{
    struct stat statbuf;
    FILE *ltfsdmd = NULL;
    char line[Const::OUTPUT_LINE_SIZE];
    int pid;
    int ret;
    int retry = 0;
    bool success = false;

    if (stat(serverPath.str().c_str(), &statbuf) == -1) {
        MSG(LTFSDMC0021E);
        TRACE(Trace::error, serverPath.str(), errno);
        THROW(Error::LTFSDM_GENERAL_ERROR);
    }

    MSG(LTFSDMC0099I);

    ltfsdmd = popen(serverPath.str().c_str(), "r");

    if (!ltfsdmd) {
        MSG(LTFSDMC0022E);
        TRACE(Trace::error, errno);
        THROW(Error::LTFSDM_GENERAL_ERROR);
    }

    while (fgets(line, sizeof(line), ltfsdmd)) {
        INFO(LTFSDMC0024I, line);
    }

    ret = pclose(ltfsdmd);

    if (!WIFEXITED(ret) || WEXITSTATUS(ret)) {
        MSG(LTFSDMC0022E);
        TRACE(Trace::error, ret, WIFEXITED(ret), WEXITSTATUS(ret));
        THROW(Error::LTFSDM_GENERAL_ERROR);
    }

    sleep(1);

    MSG(LTFSDMC0100I);
    while (retry < 10) {
        try {
            connect();
            success = true;
            break;
        } catch (const std::exception& e) {
            retry++;
            sleep(1);
        }
    }

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
        THROW(Error::LTFSDM_GENERAL_ERROR);
    }

    try {
        commCommand.recv();
    } catch (const std::exception& e) {
        MSG(LTFSDMC0098E);
        THROW(Error::LTFSDM_GENERAL_ERROR);
    }

    const LTFSDmProtocol::LTFSDmStatusResp statusresp =
            commCommand.statusresp();

    if (statusresp.success() == true) {
        pid = statusresp.pid();
        MSG(LTFSDMC0097I, pid);
    } else {
        MSG(LTFSDMC0098E);
        THROW(Error::LTFSDM_GENERAL_ERROR);
    }

}

void StartCommand::doCommand(int argc, char **argv)

{
    if (argc > 1) {
        printUsage();
        THROW(Error::LTFSDM_GENERAL_ERROR);
    }

    determineServerPath();
    startServer();
}
