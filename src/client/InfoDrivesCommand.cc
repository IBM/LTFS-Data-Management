#include <sys/resource.h>

#include <unistd.h>
#include <string>
#include <list>
#include <sstream>
#include <exception>

#include "src/common/errors/errors.h"
#include "src/common/exception/OpenLTFSException.h"
#include "src/common/messages/Message.h"
#include "src/common/tracing/Trace.h"

#include "src/common/comm/ltfsdm.pb.h"
#include "src/common/comm/LTFSDmComm.h"

#include "OpenLTFSCommand.h"
#include "InfoDrivesCommand.h"

void InfoDrivesCommand::printUsage()
{
    INFO(LTFSDMC0068I);
}

void InfoDrivesCommand::doCommand(int argc, char **argv)
{
    processOptions(argc, argv);

    TRACE(Trace::normal, *argv, argc, optind);

    if (argc != optind) {
        printUsage();
        THROW(Error::GENERAL_ERROR);
    }

    try {
        connect();
    } catch (const std::exception& e) {
        MSG(LTFSDMC0026E);
        return;
    }

    LTFSDmProtocol::LTFSDmInfoDrivesRequest *infodrives =
            commCommand.mutable_infodrivesrequest();

    infodrives->set_key(key);

    try {
        commCommand.send();
    } catch (const std::exception& e) {
        MSG(LTFSDMC0027E);
        THROW(Error::GENERAL_ERROR);
    }

    INFO(LTFSDMC0069I);

    std::string id;

    do {
        try {
            commCommand.recv();
        } catch (const std::exception& e) {
            MSG(LTFSDMC0028E);
            THROW(Error::GENERAL_ERROR);
        }

        const LTFSDmProtocol::LTFSDmInfoDrivesResp infodrivesresp =
                commCommand.infodrivesresp();
        id = infodrivesresp.id();
        std::string devname = infodrivesresp.devname();
        unsigned long slot = infodrivesresp.slot();
        std::string status = infodrivesresp.status();
        bool busy = infodrivesresp.busy();
        if (id.compare("") != 0)
            INFO(LTFSDMC0070I, id, devname, slot, status,
                    busy ? messages[LTFSDMC0071I] : messages[LTFSDMC0072I]);
    } while (id.compare("") != 0);

    return;
}
