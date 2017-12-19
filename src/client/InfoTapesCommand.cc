#include <sys/resource.h>

#include <unistd.h>
#include <string>
#include <list>
#include <sstream>
#include <exception>

#include "src/common/errors/errors.h"
#include "src/common/exception/LTFSDMException.h"
#include "src/common/messages/Message.h"
#include "src/common/tracing/Trace.h"

#include "src/common/comm/ltfsdm.pb.h"
#include "src/common/comm/LTFSDmComm.h"

#include "LTFSDMCommand.h"
#include "InfoTapesCommand.h"

/** @page ltfsdm_info_tapes ltfsdm info tapes
    The ltfsdm info tapes command lists all available cartridges and corresponding space information.

    <tt>@LTFSDMC0065I</tt>

    parameters | description
    ---|---
    - | -

    Example:

    @verbatim
    [root@visp ~]# ltfsdm info tapes
    id           slot         total cap.   rem. cap.    status       in progress  pool         state
    D01298L5     4121         0            0            Unknown      0            n/a          not mounted
    D01299L5     4134         0            0            Unknown      0            n/a          not mounted
    D01300L5     4129         0            0            Unknown      0            n/a          not mounted
    D01301L5     256          1358985      1357007      Valid LTFS   0            pool1        mounted
    D01302L5     259          1358985      1357300      Valid LTFS   0            pool2        mounted
    @endverbatim

    The responsible class is @ref InfoTapesCommand.
 */

void InfoTapesCommand::printUsage()
{
    INFO(LTFSDMC0065I);
}

void InfoTapesCommand::doCommand(int argc, char **argv)
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

    LTFSDmProtocol::LTFSDmInfoTapesRequest *infotapes =
            commCommand.mutable_infotapesrequest();

    infotapes->set_key(key);

    try {
        commCommand.send();
    } catch (const std::exception& e) {
        MSG(LTFSDMC0027E);
        THROW(Error::GENERAL_ERROR);
    }

    INFO(LTFSDMC0066I);

    std::string id;

    do {
        try {
            commCommand.recv();
        } catch (const std::exception& e) {
            MSG(LTFSDMC0028E);
            THROW(Error::GENERAL_ERROR);
        }

        const LTFSDmProtocol::LTFSDmInfoTapesResp infotapesresp =
                commCommand.infotapesresp();
        id = infotapesresp.id();
        unsigned long slot = infotapesresp.slot();
        unsigned long totalcap = infotapesresp.totalcap();
        unsigned long remaincap = infotapesresp.remaincap();
        std::string status = infotapesresp.status();
        unsigned long inprogress = infotapesresp.inprogress();
        std::string pool = infotapesresp.pool();
        if (pool.compare("") == 0)
            pool = messages[LTFSDMC0090I];
        std::string state = infotapesresp.state();
        if (id.compare("") != 0)
            INFO(LTFSDMC0067I, id, slot, totalcap, remaincap, status,
                    inprogress, pool, state);
    } while (!exitClient && id.compare("") != 0);

    return;
}
