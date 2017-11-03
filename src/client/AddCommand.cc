#include <fcntl.h>
#include <sys/file.h>
#include <sys/resource.h>
#include <libmount/libmount.h>
#include <blkid/blkid.h>

#include <string>
#include <set>
#include <vector>
#include <list>
#include <sstream>
#include <exception>

#include "src/common/errors/errors.h"
#include "src/common/exception/OpenLTFSException.h"
#include "src/common/util/util.h"
#include "src/common/util/FileSystems.h"
#include "src/common/messages/Message.h"
#include "src/common/tracing/Trace.h"

#include "src/common/comm/ltfsdm.pb.h"
#include "src/common/comm/LTFSDmComm.h"

#include "OpenLTFSCommand.h"
#include "AddCommand.h"

void AddCommand::printUsage()
{
    INFO(LTFSDMC0052I);
}

void AddCommand::doCommand(int argc, char **argv)
{
    char *pathName = NULL;
    std::string managedFs;

    if (argc == 1) {
        printUsage();
        THROW(Error::GENERAL_ERROR);
    }

    processOptions(argc, argv);

    pathName = canonicalize_file_name(argv[optind]);

    if (pathName == NULL) {
        MSG(LTFSDMC0053E);
        THROW(Error::GENERAL_ERROR);
    }

    managedFs = pathName;

    try {
        FileSystems fss;
        FileSystems::fsinfo fs = fss.getByTarget(managedFs);
    } catch ( const std::exception& e ) {
        MSG(LTFSDMC0053E);
        THROW(Error::GENERAL_ERROR);
    }

    if (argc != optind + 1) {
        printUsage();
        THROW(Error::GENERAL_ERROR);
    }

    try {
        connect();
    } catch (const std::exception& e) {
        MSG(LTFSDMC0026E);
        THROW(Error::GENERAL_ERROR);
    }

    TRACE(Trace::normal, requestNumber);

    LTFSDmProtocol::LTFSDmAddRequest *addreq = commCommand.mutable_addrequest();
    addreq->set_key(key);
    addreq->set_reqnumber(requestNumber);
    addreq->set_managedfs(managedFs);
    addreq->set_mountpoint(mountPoint);
    addreq->set_fsname(fsName);

    try {
        commCommand.send();
    } catch (const std::exception& e) {
        MSG(LTFSDMC0027E);
        THROW(Error::GENERAL_ERROR);
    }

    try {
        commCommand.recv();
    } catch (const std::exception& e) {
        MSG(LTFSDMC0028E);
        THROW(Error::GENERAL_ERROR);
    }

    const LTFSDmProtocol::LTFSDmAddResp addresp = commCommand.addresp();

    if (addresp.response() == LTFSDmProtocol::LTFSDmAddResp::FAILED) {
        MSG(LTFSDMC0055E, managedFs);
        THROW(Error::GENERAL_ERROR);
    } else if (addresp.response()
            == LTFSDmProtocol::LTFSDmAddResp::ALREADY_ADDED) {
        MSG(LTFSDMC0054I, managedFs);
    }
}
