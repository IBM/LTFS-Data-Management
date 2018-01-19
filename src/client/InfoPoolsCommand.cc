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
#include "InfoPoolsCommand.h"

/** @page ltfsdm_info_pools ltfsdm info pools
    The ltfsdm info pools command provides information about tape storage pools and the number of assigned tapes.

    <tt>@LTFSDMC0087I</tt>

    parameters | description
    ---|---
    - | -

    Example:

    @verbatim
    [root@visp ~]# ltfsdm info pools
    pool name    total cap.   rem. cap.    unref. cap.  #tapes
    large pool   0            0            0            3
    pool1        1358985      1357012      0            1
    pool2        1358985      1357300      0            1
    @endverbatim

    The corresponding class is @ref InfoPoolsCommand.
 */

void InfoPoolsCommand::printUsage()
{
    INFO(LTFSDMC0087I);
}

void InfoPoolsCommand::doCommand(int argc, char **argv)
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

    LTFSDmProtocol::LTFSDmInfoPoolsRequest *infopools =
            commCommand.mutable_infopoolsrequest();

    infopools->set_key(key);

    try {
        commCommand.send();
    } catch (const std::exception& e) {
        MSG(LTFSDMC0027E);
        THROW(Error::GENERAL_ERROR);
    }

    INFO(LTFSDMC0088I);

    std::string name;

    do {
        try {
            commCommand.recv();
        } catch (const std::exception& e) {
            MSG(LTFSDMC0028E);
            THROW(Error::GENERAL_ERROR);
        }

        const LTFSDmProtocol::LTFSDmInfoPoolsResp infopoolsresp =
                commCommand.infopoolsresp();
        name = infopoolsresp.poolname();
        unsigned long total = infopoolsresp.total();
        unsigned long free = infopoolsresp.free();
        unsigned long unref = infopoolsresp.unref();
        unsigned long numtapes = infopoolsresp.numtapes();
        if (name.compare("") != 0)
            INFO(LTFSDMC0089I, name, total, free, unref, numtapes);
    } while (name.compare("") != 0);

    return;
}
