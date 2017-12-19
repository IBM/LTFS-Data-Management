#include <linux/fs.h>
#include <dirent.h>
#include <sys/resource.h>
#include <libmount/libmount.h>
#include <blkid/blkid.h>

#include <sstream>
#include <map>
#include <set>
#include <vector>
#include <thread>

#include "src/common/errors/errors.h"
#include "src/common/exception/LTFSDMException.h"
#include "src/common/util/util.h"
#include "src/common/util/FileSystems.h"
#include "src/common/messages/Message.h"
#include "src/common/tracing/Trace.h"
#include "src/common/const/Const.h"
#include "src/common/configuration/Configuration.h"

#include "src/common/comm/ltfsdm.pb.h"
#include "src/common/comm/LTFSDmComm.h"

#include "src/connector/fuse/FuseLock.h"
#include "src/connector/fuse/FuseFS.h"
#include "src/connector/fuse/FuseConnector.h"
#include "src/connector/Connector.h"

/** @page fuse_connector Fuse connector

 */

std::atomic<bool> Connector::connectorTerminate(false);
std::atomic<bool> Connector::forcedTerminate(false);
std::atomic<bool> Connector::recallEventSystemStopped(false);
Configuration *Connector::conf = nullptr;

LTFSDmCommServer recrequest(Const::RECALL_SOCKET_FILE);

Connector::Connector(bool _cleanup, Configuration *_conf) :
        cleanup(_cleanup)

{
    clock_gettime(CLOCK_REALTIME, &starttime);
    FuseConnector::ltfsdmKey = LTFSDM::getkey();
    conf = _conf;
}

Connector::~Connector()

{
    if ( cleanup ) {
        try {
            std::string mountpt;

            MSG(LTFSDMF0025I);
            FuseConnector::managedFss.clear();
            MSG(LTFSDMF0027I);
        } catch (const std::exception& e) {
            TRACE(Trace::error, e.what());
            kill(getpid(), SIGTERM);
        }
    }
}

void Connector::initTransRecalls()

{
    try {
        recrequest.listen();
    } catch (const std::exception& e) {
        TRACE(Trace::error, e.what());
        MSG(LTFSDMF0026E);
        THROW(Error::GENERAL_ERROR);
    }
}

void Connector::endTransRecalls()

{
    recrequest.closeRef();
}

Connector::rec_info_t Connector::getEvents()

{
    Connector::rec_info_t recinfo;
    long key;

    recrequest.accept();

    try {
        recrequest.recv();
    } catch (const std::exception& e) {
        MSG(LTFSDMF0019E, e.what(), errno);
        THROW(Error::GENERAL_ERROR);
    }

    const LTFSDmProtocol::LTFSDmTransRecRequest request =
            recrequest.transrecrequest();

    key = request.key();
    if (FuseConnector::ltfsdmKey != key) {
        TRACE(Trace::error, (long ) FuseConnector::ltfsdmKey, key);
        recinfo = (rec_info_t ) { NULL, false, (fuid_t ) { 0, 0, 0, 0 }, "" };
        return recinfo;
    }

    struct conn_info_t *conn_info = new struct conn_info_t;
    conn_info->reqrequest = new LTFSDmCommServer(recrequest);

    recinfo.conn_info = conn_info;
    recinfo.toresident = request.toresident();
    recinfo.fuid = (fuid_t ) { (unsigned long) request.fsidh(),
                    (unsigned long) request.fsidl(),
                    (unsigned int) request.igen(),
                    (unsigned long) request.inum() };
    recinfo.filename = request.filename();

    TRACE(Trace::always, recinfo.filename, recinfo.fuid.inum,
            recinfo.toresident);

    return recinfo;
}

void Connector::respondRecallEvent(rec_info_t recinfo, bool success)

{
    LTFSDmProtocol::LTFSDmTransRecResp *trecresp =
            recinfo.conn_info->reqrequest->mutable_transrecresp();

    trecresp->set_success(success);

    try {
        recinfo.conn_info->reqrequest->send();
    } catch (const std::exception& e) {
        TRACE(Trace::error, e.what());
        MSG(LTFSDMS0007E);
    }

    TRACE(Trace::always, recinfo.filename, success);

    recinfo.conn_info->reqrequest->closeAcc();

    delete (recinfo.conn_info->reqrequest);
    delete (recinfo.conn_info);
}

void Connector::terminate()

{
    Connector::connectorTerminate = true;

    LTFSDmCommClient commCommand(Const::RECALL_SOCKET_FILE);

    try {
        commCommand.connect();
    } catch (const std::exception& e) {
        MSG(LTFSDMF0020E, e.what(), errno);
        THROW(Error::GENERAL_ERROR);
    }

    LTFSDmProtocol::LTFSDmTransRecRequest *recrequest =
            commCommand.mutable_transrecrequest();

    recrequest->set_key(FuseConnector::ltfsdmKey);
    recrequest->set_toresident(false);
    recrequest->set_fsidh(0);
    recrequest->set_fsidl(0);
    recrequest->set_igen(0);
    recrequest->set_inum(0);
    recrequest->set_filename("");

    try {
        commCommand.send();
    } catch (const std::exception& e) {
        MSG(LTFSDMF0024E);
        THROW(Error::GENERAL_ERROR, errno);
    }
}

