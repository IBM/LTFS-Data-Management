#include "ServerIncludes.h"

void MessageParser::getObjects(LTFSDmCommServer *command, long localReqNumber,
        unsigned long pid, long requestNumber, FileOperation *fopt)

{
    bool cont = true;
    bool success = true;

    TRACE(Trace::full, __PRETTY_FUNCTION__);

    while (cont) {
        if (Server::forcedTerminate)
            return;

        try {
            command->recv();
        } catch (const std::exception& e) {
            TRACE(Trace::error, e.what());
            MSG(LTFSDMS0006E);
            THROW(Const::UNSET);
        }

        if (!command->has_sendobjects()) {
            TRACE(Trace::error, command->has_sendobjects());
            MSG(LTFSDMS0011E);
            return;
        }

        const LTFSDmProtocol::LTFSDmSendObjects sendobjects =
                command->sendobjects();

        for (int j = 0; j < sendobjects.filenames_size(); j++) {
            if (Server::terminate == true) {
                command->closeAcc();
                return;
            }
            const LTFSDmProtocol::LTFSDmSendObjects::FileName& filename =
                    sendobjects.filenames(j);
            if (filename.filename().compare("") != 0) {
                try {
                    fopt->addJob(filename.filename());
                } catch (const OpenLTFSException& e) {
                    TRACE(Trace::error, e.what());
                    if (e.getError() == SQLITE_CONSTRAINT_PRIMARYKEY
                            || e.getError() == SQLITE_CONSTRAINT_UNIQUE)
                        MSG(LTFSDMS0019E, filename.filename().c_str());
                    else
                        MSG(LTFSDMS0015E, filename.filename().c_str(),
                                sqlite3_errstr(e.getError()));
                } catch (const std::exception& e) {
                    TRACE(Trace::error, e.what());
                }
            } else {
                cont = false; // END
            }
        }

        LTFSDmProtocol::LTFSDmSendObjectsResp *sendobjresp =
                command->mutable_sendobjectsresp();

        sendobjresp->set_success(success);
        sendobjresp->set_reqnumber(requestNumber);
        sendobjresp->set_pid(pid);

        try {
            command->send();
        } catch (const std::exception& e) {
            TRACE(Trace::error, e.what());
            MSG(LTFSDMS0007E);
            return;
        }
    }
}

void MessageParser::reqStatusMessage(long key, LTFSDmCommServer *command,
        FileOperation *fopt)

{
    TRACE(Trace::always, __PRETTY_FUNCTION__);

    long resident = 0;
    long premigrated = 0;
    long migrated = 0;
    long failed = 0;
    bool done;
    unsigned long pid;
    long requestNumber;
    long keySent;

    do {
        try {
            command->recv();
        } catch (const std::exception& e) {
            TRACE(Trace::error, e.what());
            MSG(LTFSDMS0006E);
            return;
        }

        const LTFSDmProtocol::LTFSDmReqStatusRequest reqstatus =
                command->reqstatusrequest();

        keySent = reqstatus.key();
        if (key != keySent) {
            MSG(LTFSDMS0008E, keySent);
            return;
        }

        requestNumber = reqstatus.reqnumber();
        pid = reqstatus.pid();

        done = fopt->queryResult(requestNumber, &resident, &premigrated,
                &migrated, &failed);

        LTFSDmProtocol::LTFSDmReqStatusResp *reqstatusresp =
                command->mutable_reqstatusresp();

        reqstatusresp->set_success(true);
        reqstatusresp->set_reqnumber(requestNumber);
        reqstatusresp->set_pid(pid);
        reqstatusresp->set_resident(resident);
        reqstatusresp->set_premigrated(premigrated);
        reqstatusresp->set_migrated(migrated);
        reqstatusresp->set_failed(failed);
        reqstatusresp->set_done(done);

        try {
            command->send();
        } catch (const std::exception& e) {
            TRACE(Trace::error, e.what());
            MSG(LTFSDMS0007E);
            return;
        }
    } while (!done);
}

void MessageParser::migrationMessage(long key, LTFSDmCommServer *command,
        long localReqNumber)

{
    TRACE(Trace::always, __PRETTY_FUNCTION__);

    unsigned long pid;
    long requestNumber;
    const LTFSDmProtocol::LTFSDmMigRequest migreq = command->migrequest();
    long keySent = migreq.key();
    std::set<std::string> pools;
    std::string pool;
    int error = Error::LTFSDM_OK;
    Migration *mig = nullptr;

    TRACE(Trace::normal, keySent);

    if (key != keySent) {
        MSG(LTFSDMS0008E, keySent);
        return;
    }

    requestNumber = migreq.reqnumber();
    pid = migreq.pid();

    if (Server::terminate == false) {
        std::stringstream poolss(migreq.pools());

        {
            std::lock_guard<std::recursive_mutex> lock(OpenLTFSInventory::mtx);
            while (std::getline(poolss, pool, ',')) {
                std::shared_ptr<OpenLTFSPool> poolp = inventory->getPool(pool);
                if (poolp == nullptr) {
                    error = Error::LTFSDM_NOT_ALL_POOLS_EXIST;
                    break;
                }
                if (pools.count(pool) == 0)
                    pools.insert(pool);
            }
        }

        if (!error && (pools.size() > 3)) {
            error = Error::LTFSDM_WRONG_POOLNUM;
        }

        mig = new Migration(pid, requestNumber, pools, pools.size(),
                migreq.state());
    } else {
        error = Error::LTFSDM_TERMINATING;
    }

    LTFSDmProtocol::LTFSDmMigRequestResp *migreqresp =
            command->mutable_migrequestresp();

    migreqresp->set_error(error);
    migreqresp->set_reqnumber(requestNumber);
    migreqresp->set_pid(pid);

    try {
        command->send();
    } catch (const std::exception& e) {
        TRACE(Trace::error, e.what());
        MSG(LTFSDMS0007E);
        return;
    }

    if (!error) {
        try {
            getObjects(command, localReqNumber, pid, requestNumber,
                    dynamic_cast<FileOperation*>(mig));
        } catch (const std::exception& e) {
            SQLStatement stmt;
            stmt(FileOperation::DELETE_JOBS) << requestNumber;
            stmt.doall();
            return;
        }
        mig->addRequest();
        reqStatusMessage(key, command, dynamic_cast<FileOperation*>(mig));
    }

    if (mig != nullptr)
        delete (mig);
}

void MessageParser::selRecallMessage(long key, LTFSDmCommServer *command,
        long localReqNumber)

{
    TRACE(Trace::always, __PRETTY_FUNCTION__);
    unsigned long pid;
    long requestNumber;
    const LTFSDmProtocol::LTFSDmSelRecRequest recreq = command->selrecrequest();
    long keySent = recreq.key();
    int error = Error::LTFSDM_OK;
    SelRecall *srec = nullptr;

    TRACE(Trace::normal, keySent);

    if (key != keySent) {
        MSG(LTFSDMS0008E, keySent);
        return;
    }

    requestNumber = recreq.reqnumber();
    pid = recreq.pid();

    if (Server::terminate == false)
        srec = new SelRecall(pid, requestNumber, recreq.state());
    else
        error = Error::LTFSDM_TERMINATING;

    LTFSDmProtocol::LTFSDmSelRecRequestResp *recreqresp =
            command->mutable_selrecrequestresp();

    recreqresp->set_error(error);
    recreqresp->set_reqnumber(requestNumber);
    recreqresp->set_pid(pid);

    try {
        command->send();
    } catch (const std::exception& e) {
        TRACE(Trace::error, e.what());
        MSG(LTFSDMS0007E);
        return;
    }

    if (!error) {
        try {
            getObjects(command, localReqNumber, pid, requestNumber,
                    dynamic_cast<FileOperation*>(srec));
        } catch (const std::exception& e) {
            SQLStatement stmt;
            stmt(FileOperation::DELETE_JOBS) << requestNumber;
            stmt.doall();
            return;
        }
        srec->addRequest();
        reqStatusMessage(key, command, dynamic_cast<FileOperation*>(srec));
    }

    if (srec != nullptr)
        delete (srec);
}

void MessageParser::requestNumber(long key, LTFSDmCommServer *command,
        long *localReqNumber)

{
    TRACE(Trace::always, __PRETTY_FUNCTION__);

    const LTFSDmProtocol::LTFSDmReqNumber reqnum = command->reqnum();
    long keySent = reqnum.key();

    TRACE(Trace::normal, keySent);

    if (key != keySent) {
        MSG(LTFSDMS0008E, keySent);
        return;
    }

    LTFSDmProtocol::LTFSDmReqNumberResp *reqnumresp =
            command->mutable_reqnumresp();

    reqnumresp->set_success(true);
    *localReqNumber = ++globalReqNumber;
    reqnumresp->set_reqnumber(*localReqNumber);

    TRACE(Trace::normal, *localReqNumber);

    try {
        command->send();
    } catch (const std::exception& e) {
        TRACE(Trace::error, e.what());
        MSG(LTFSDMS0007E);
    }

}

void MessageParser::stopMessage(long key, LTFSDmCommServer *command,
        std::unique_lock<std::mutex> *reclock, long localReqNumber)

{
    TRACE(Trace::always, __PRETTY_FUNCTION__);
    const LTFSDmProtocol::LTFSDmStopRequest stopreq = command->stoprequest();
    long keySent = stopreq.key();
    SQLStatement stmt;
    int state;
    int numreqs;

    TRACE(Trace::normal, keySent);

    if (key != keySent) {
        MSG(LTFSDMS0008E, keySent);
        return;
    }

    MSG(LTFSDMS0009I);

    Server::terminate = true;

    if (stopreq.forced()) {
        Server::forcedTerminate = true;
        Connector::forcedTerminate = true;
    }

    if (stopreq.finish()) {
        Server::finishTerminate = true;
        std::lock_guard<std::mutex> updlock(Scheduler::updmtx);
        Scheduler::updcond.notify_all();
    }

    Server::termcond.notify_one();
    reclock->unlock();

    do {
        numreqs = 0;

        if (Server::forcedTerminate == false && Server::finishTerminate == false) {
            stmt(MessageParser::ALL_REQUESTS);
            stmt.prepare();
            while (stmt.step(&state)) {
                if (state == DataBase::REQ_INPROGRESS) {
                    numreqs++;
                }
            }
            stmt.finalize();
            TRACE(Trace::always, numreqs);
        }

        LTFSDmProtocol::LTFSDmStopResp *stopresp = command->mutable_stopresp();

        stopresp->set_success(numreqs == 0);

        try {
            command->send();
        } catch (const std::exception& e) {
            TRACE(Trace::error, e.what());
            MSG(LTFSDMS0007E);
            return;
        }

        if (numreqs > 0) {
            try {
                command->recv();
            } catch (const std::exception& e) {
                TRACE(Trace::error, e.what());
                MSG(LTFSDMS0006E);
                return;
            }
        }
    } while (numreqs > 0);

    TRACE(Trace::always, numreqs);

    std::unique_lock<std::mutex> lock(Scheduler::mtx);
    Scheduler::cond.notify_one();
    lock.unlock();

    kill(getpid(), SIGUSR1);
}

void MessageParser::statusMessage(long key, LTFSDmCommServer *command,
        long localReqNumber)

{
    TRACE(Trace::always, __PRETTY_FUNCTION__);
    const LTFSDmProtocol::LTFSDmStatusRequest statusreq =
            command->statusrequest();
    long keySent = statusreq.key();

    TRACE(Trace::normal, keySent);

    if (key != keySent) {
        MSG(LTFSDMS0008E, keySent);
        return;
    }

    LTFSDmProtocol::LTFSDmStatusResp *statusresp =
            command->mutable_statusresp();

    statusresp->set_success(true);

    statusresp->set_pid(getpid());

    try {
        command->send();
    } catch (const std::exception& e) {
        TRACE(Trace::error, e.what());
        MSG(LTFSDMS0007E);
    }
}

void MessageParser::addMessage(long key, LTFSDmCommServer *command,
        long localReqNumber, Connector *connector)

{
    TRACE(Trace::always, __PRETTY_FUNCTION__);
    const LTFSDmProtocol::LTFSDmAddRequest addreq = command->addrequest();
    long keySent = addreq.key();
    std::string managedFs = addreq.managedfs();
    std::string mountPoint = addreq.mountpoint();
    std::string fsName = addreq.fsname();
    LTFSDmProtocol::LTFSDmAddResp_AddResp response =
            LTFSDmProtocol::LTFSDmAddResp::SUCCESS;

    TRACE(Trace::normal, keySent);

    if (key != keySent) {
        MSG(LTFSDMS0008E, keySent);
        return;
    }

    try {
        FsObj fileSystem(managedFs);

        if (fileSystem.isFsManaged()) {
            MSG(LTFSDMS0043W, managedFs);
            response = LTFSDmProtocol::LTFSDmAddResp::ALREADY_ADDED;
        } else {
            MSG(LTFSDMS0042I, managedFs);
            fileSystem.manageFs(true, connector->getStartTime(), mountPoint,
                    fsName);
        }
    } catch (const OpenLTFSException& e) {
        response = LTFSDmProtocol::LTFSDmAddResp::FAILED;
        TRACE(Trace::error, e.what());
        switch (e.getError()) {
            case Error::LTFSDM_FS_CHECK_ERROR:
                MSG(LTFSDMS0044E, managedFs);
                break;
            case Error::LTFSDM_FS_ADD_ERROR:
                MSG(LTFSDMS0045E, managedFs);
                break;
            default:
                MSG(LTFSDMS0045E, managedFs);
        }
    } catch (const std::exception& e) {
        TRACE(Trace::error, e.what());
    }

    LTFSDmProtocol::LTFSDmAddResp *addresp = command->mutable_addresp();

    addresp->set_response(response);

    try {
        command->send();
    } catch (const std::exception& e) {
        MSG(LTFSDMS0007E);
    }
}

void MessageParser::infoRequestsMessage(long key, LTFSDmCommServer *command,
        long localReqNumber)

{
    TRACE(Trace::always, __PRETTY_FUNCTION__);
    const LTFSDmProtocol::LTFSDmInfoRequestsRequest inforeqs =
            command->inforequestsrequest();
    long keySent = inforeqs.key();
    int requestNumber = inforeqs.reqnumber();
    SQLStatement stmt;
    std::stringstream ssql;
    DataBase::operation op;
    int reqNum;
    std::string tapeId;
    DataBase::req_state tgtstate;
    DataBase::req_state state;
    std::string pool;

    TRACE(Trace::normal, keySent);

    if (key != keySent) {
        MSG(LTFSDMS0008E, keySent);
        return;
    }

    TRACE(Trace::normal, requestNumber);

    if (requestNumber != Const::UNSET)
        stmt(MessageParser::INFO_ONE_REQUEST) << requestNumber;
    else
        stmt(MessageParser::INFO_ALL_REQUESTS);

    stmt.prepare();

    while (stmt.step(&op, &reqNum, &tapeId, &tgtstate, &state, &pool)) {
        LTFSDmProtocol::LTFSDmInfoRequestsResp *inforeqsresp =
                command->mutable_inforequestsresp();

        inforeqsresp->set_operation(DataBase::opStr(op));
        inforeqsresp->set_reqnumber(reqNum);
        inforeqsresp->set_tapeid(tapeId);
        inforeqsresp->set_targetstate(DataBase::reqStateStr(tgtstate));
        inforeqsresp->set_state(DataBase::reqStateStr(state));
        inforeqsresp->set_pool(pool);

        try {
            command->send();
        } catch (const std::exception& e) {
            TRACE(Trace::error, e.what());
            MSG(LTFSDMS0007E);
            stmt.finalize();
            return;
        }
    }
    stmt.finalize();

    LTFSDmProtocol::LTFSDmInfoRequestsResp *inforeqsresp =
            command->mutable_inforequestsresp();
    inforeqsresp->set_operation("");
    inforeqsresp->set_reqnumber(Const::UNSET);
    inforeqsresp->set_tapeid("");
    inforeqsresp->set_targetstate("");
    inforeqsresp->set_state("");
    inforeqsresp->set_pool("");

    try {
        command->send();
    } catch (const std::exception& e) {
        TRACE(Trace::error, e.what());
        MSG(LTFSDMS0007E);
    }
}

void MessageParser::infoJobsMessage(long key, LTFSDmCommServer *command,
        long localReqNumber)

{
    TRACE(Trace::always, __PRETTY_FUNCTION__);
    const LTFSDmProtocol::LTFSDmInfoJobsRequest infojobs =
            command->infojobsrequest();
    long keySent = infojobs.key();
    int requestNumber = infojobs.reqnumber();
    SQLStatement stmt;
    DataBase::operation op;
    std::string fileName;
    int reqNum;
    std::string pool;
    unsigned long fileSize;
    std::string tapeId;
    FsObj::file_state state;

    TRACE(Trace::normal, keySent);

    if (key != keySent) {
        MSG(LTFSDMS0008E, keySent);
        return;
    }

    TRACE(Trace::normal, requestNumber);

    if (requestNumber != Const::UNSET)
        stmt(MessageParser::INFO_SEL_JOBS) << requestNumber;
    else
        stmt(MessageParser::INFO_ALL_JOBS);

    stmt.prepare();

    while (stmt.step(&op, &fileName, &reqNum, &pool, &fileSize, &tapeId, &state)) {
        LTFSDmProtocol::LTFSDmInfoJobsResp *infojobsresp =
                command->mutable_infojobsresp();

        infojobsresp->set_operation(DataBase::opStr(op));
        infojobsresp->set_filename(fileName);
        infojobsresp->set_reqnumber(reqNum);
        infojobsresp->set_pool(pool);
        infojobsresp->set_filesize(fileSize);
        infojobsresp->set_tapeid(tapeId);
        infojobsresp->set_state(FsObj::migStateStr(state));

        try {
            command->send();
        } catch (const std::exception& e) {
            TRACE(Trace::error, e.what());
            MSG(LTFSDMS0007E);
            stmt.finalize();
            return;
        }
    }
    stmt.finalize();

    LTFSDmProtocol::LTFSDmInfoJobsResp *infojobsresp =
            command->mutable_infojobsresp();
    infojobsresp->set_operation("");
    infojobsresp->set_filename("");
    infojobsresp->set_reqnumber(Const::UNSET);
    infojobsresp->set_pool("");
    infojobsresp->set_filesize(Const::UNSET);
    infojobsresp->set_tapeid("");
    infojobsresp->set_state("");

    try {
        command->send();
    } catch (const std::exception& e) {
        TRACE(Trace::error, e.what());
        MSG(LTFSDMS0007E);
    }
}

void MessageParser::infoDrivesMessage(long key, LTFSDmCommServer *command)

{
    TRACE(Trace::always, __PRETTY_FUNCTION__);
    const LTFSDmProtocol::LTFSDmInfoDrivesRequest infodrives =
            command->infodrivesrequest();
    long keySent = infodrives.key();

    TRACE(Trace::normal, keySent);

    if (key != keySent) {
        MSG(LTFSDMS0008E, keySent);
        return;
    }

    {
        std::lock_guard<std::recursive_mutex> lock(OpenLTFSInventory::mtx);
        for (std::shared_ptr<OpenLTFSDrive> d : inventory->getDrives()) {
            LTFSDmProtocol::LTFSDmInfoDrivesResp *infodrivesresp =
                    command->mutable_infodrivesresp();

            infodrivesresp->set_id(d->GetObjectID());
            infodrivesresp->set_devname(d->get_devname());
            infodrivesresp->set_slot(d->get_slot());
            infodrivesresp->set_status(d->get_status());
            infodrivesresp->set_busy(d->isBusy());

            try {
                command->send();
            } catch (const std::exception& e) {
                TRACE(Trace::error, e.what());
                MSG(LTFSDMS0007E);
                return;
            }
        }
    }

    LTFSDmProtocol::LTFSDmInfoDrivesResp *infodrivesresp =
            command->mutable_infodrivesresp();

    infodrivesresp->set_id("");
    infodrivesresp->set_devname("");
    infodrivesresp->set_slot(0);
    infodrivesresp->set_status("");
    infodrivesresp->set_busy(false);

    try {
        command->send();
    } catch (const std::exception& e) {
        TRACE(Trace::error, e.what());
        MSG(LTFSDMS0007E);
    }
}

void MessageParser::infoTapesMessage(long key, LTFSDmCommServer *command)

{
    TRACE(Trace::always, __PRETTY_FUNCTION__);
    const LTFSDmProtocol::LTFSDmInfoTapesRequest infotapes =
            command->infotapesrequest();
    long keySent = infotapes.key();
    std::string state;

    TRACE(Trace::normal, keySent);

    if (key != keySent) {
        MSG(LTFSDMS0008E, keySent);
        return;
    }

    {
        std::lock_guard<std::recursive_mutex> lock(OpenLTFSInventory::mtx);
        for (std::shared_ptr<OpenLTFSCartridge> c : inventory->getCartridges()) {
            LTFSDmProtocol::LTFSDmInfoTapesResp *infotapesresp =
                    command->mutable_infotapesresp();

            infotapesresp->set_id(c->GetObjectID());
            infotapesresp->set_slot(c->get_slot());
            infotapesresp->set_totalcap(c->get_total_cap());
            infotapesresp->set_remaincap(c->get_remaining_cap());
            infotapesresp->set_status(c->get_status());
            infotapesresp->set_inprogress(c->getInProgress());
            infotapesresp->set_pool(c->getPool());
            switch (c->getState()) {
                case OpenLTFSCartridge::INUSE:
                    state = messages[LTFSDMS0055I];
                    break;
                case OpenLTFSCartridge::MOUNTED:
                    state = messages[LTFSDMS0056I];
                    break;
                case OpenLTFSCartridge::MOVING:
                    state = messages[LTFSDMS0057I];
                    break;
                case OpenLTFSCartridge::UNMOUNTED:
                    state = messages[LTFSDMS0058I];
                    break;
                case OpenLTFSCartridge::INVALID:
                    state = messages[LTFSDMS0059I];
                    break;
                case OpenLTFSCartridge::UNKNOWN:
                    state = messages[LTFSDMS0060I];
                    break;
                default:
                    state = "-";
            }

            infotapesresp->set_state(state);

            try {
                command->send();
            } catch (const std::exception& e) {
                TRACE(Trace::error, e.what());
                MSG(LTFSDMS0007E);
                return;
            }
        }
    }

    LTFSDmProtocol::LTFSDmInfoTapesResp *infotapesresp =
            command->mutable_infotapesresp();

    infotapesresp->set_id("");
    infotapesresp->set_slot(0);
    infotapesresp->set_totalcap(0);
    infotapesresp->set_remaincap(0);
    infotapesresp->set_status("");
    infotapesresp->set_inprogress(0);
    infotapesresp->set_pool("");
    infotapesresp->set_status("");

    try {
        command->send();
    } catch (const std::exception& e) {
        TRACE(Trace::error, e.what());
        MSG(LTFSDMS0007E);
    }
}

void MessageParser::poolCreateMessage(long key, LTFSDmCommServer *command)

{
    TRACE(Trace::always, __PRETTY_FUNCTION__);
    const LTFSDmProtocol::LTFSDmPoolCreateRequest poolcreate =
            command->poolcreaterequest();
    long keySent = poolcreate.key();
    std::string poolName;
    int response = Error::LTFSDM_OK;

    TRACE(Trace::normal, keySent);

    if (key != keySent) {
        MSG(LTFSDMS0008E, keySent);
        return;
    }

    poolName = poolcreate.poolname();

    {
        std::lock_guard<std::recursive_mutex> lock(OpenLTFSInventory::mtx);
        try {
            inventory->poolCreate(poolName);
            inventory->writePools();
        } catch (const OpenLTFSException& e) {
            response = e.getError();
        } catch (const std::exception& e) {
            response = Const::UNSET;
        }
    }

    LTFSDmProtocol::LTFSDmPoolResp *poolresp = command->mutable_poolresp();

    poolresp->set_response(response);

    try {
        command->send();
    } catch (const std::exception& e) {
        TRACE(Trace::error, e.what());
        MSG(LTFSDMS0007E);
    }
}

void MessageParser::poolDeleteMessage(long key, LTFSDmCommServer *command)

{
    TRACE(Trace::always, __PRETTY_FUNCTION__);
    const LTFSDmProtocol::LTFSDmPoolDeleteRequest pooldelete =
            command->pooldeleterequest();
    long keySent = pooldelete.key();
    std::string poolName;
    int response = Error::LTFSDM_OK;

    TRACE(Trace::normal, keySent);

    if (key != keySent) {
        MSG(LTFSDMS0008E, keySent);
        return;
    }

    poolName = pooldelete.poolname();

    {
        std::lock_guard<std::recursive_mutex> lock(OpenLTFSInventory::mtx);
        try {
            inventory->poolDelete(poolName);
            inventory->writePools();
        } catch (const OpenLTFSException& e) {
            response = e.getError();
        } catch (const std::exception& e) {
            response = Const::UNSET;
        }
    }

    LTFSDmProtocol::LTFSDmPoolResp *poolresp = command->mutable_poolresp();

    poolresp->set_response(response);

    try {
        command->send();
    } catch (const std::exception& e) {
        TRACE(Trace::error, e.what());
        MSG(LTFSDMS0007E);
    }
}

void MessageParser::poolAddMessage(long key, LTFSDmCommServer *command)

{
    TRACE(Trace::always, __PRETTY_FUNCTION__);
    const LTFSDmProtocol::LTFSDmPoolAddRequest pooladd =
            command->pooladdrequest();
    long keySent = pooladd.key();
    std::string poolName;
    std::list<std::string> tapeids;
    int response;

    TRACE(Trace::normal, keySent);

    if (key != keySent) {
        MSG(LTFSDMS0008E, keySent);
        return;
    }

    poolName = pooladd.poolname();

    for (int i = 0; i < pooladd.tapeid_size(); i++)
        tapeids.push_back(pooladd.tapeid(i));

    for (std::string tapeid : tapeids) {
        response = Error::LTFSDM_OK;

        {
            std::lock_guard<std::recursive_mutex> lock(OpenLTFSInventory::mtx);
            try {
                inventory->poolAdd(poolName, tapeid);
                inventory->writePools();
            } catch (const OpenLTFSException& e) {
                response = e.getError();
            } catch (const std::exception& e) {
                response = Const::UNSET;
            }
        }

        LTFSDmProtocol::LTFSDmPoolResp *poolresp = command->mutable_poolresp();

        poolresp->set_tapeid(tapeid);
        poolresp->set_response(response);

        try {
            command->send();
        } catch (const std::exception& e) {
            TRACE(Trace::error, e.what());
            MSG(LTFSDMS0007E);
        }
    }
}

void MessageParser::poolRemoveMessage(long key, LTFSDmCommServer *command)

{
    TRACE(Trace::always, __PRETTY_FUNCTION__);
    const LTFSDmProtocol::LTFSDmPoolRemoveRequest poolremove =
            command->poolremoverequest();
    long keySent = poolremove.key();
    std::string poolName;
    std::list<std::string> tapeids;
    int response;

    TRACE(Trace::normal, keySent);

    if (key != keySent) {
        MSG(LTFSDMS0008E, keySent);
        return;
    }

    poolName = poolremove.poolname();

    for (int i = 0; i < poolremove.tapeid_size(); i++)
        tapeids.push_back(poolremove.tapeid(i));

    for (std::string tapeid : tapeids) {
        response = Error::LTFSDM_OK;

        {
            std::lock_guard<std::recursive_mutex> lock(OpenLTFSInventory::mtx);
            try {
                inventory->poolRemove(poolName, tapeid);
                inventory->writePools();
            } catch (const OpenLTFSException& e) {
                response = e.getError();
            } catch (const std::exception& e) {
                response = Const::UNSET;
            }
        }

        LTFSDmProtocol::LTFSDmPoolResp *poolresp = command->mutable_poolresp();

        poolresp->set_tapeid(tapeid);
        poolresp->set_response(response);

        try {
            command->send();
        } catch (const std::exception& e) {
            TRACE(Trace::error, e.what());
            MSG(LTFSDMS0007E);
        }
    }
}

void MessageParser::infoPoolsMessage(long key, LTFSDmCommServer *command)

{
    TRACE(Trace::always, __PRETTY_FUNCTION__);
    const LTFSDmProtocol::LTFSDmInfoPoolsRequest infopools =
            command->infopoolsrequest();
    long keySent = infopools.key();
    std::string state;

    TRACE(Trace::normal, keySent);

    if (key != keySent) {
        MSG(LTFSDMS0008E, keySent);
        return;
    }

    {
        std::lock_guard<std::recursive_mutex> lock(OpenLTFSInventory::mtx);
        for (std::shared_ptr<OpenLTFSPool> pool : inventory->getPools()) {
            int numCartridges = 0;
            unsigned long total = 0;
            unsigned long free = 0;
            unsigned long unref = 0;

            LTFSDmProtocol::LTFSDmInfoPoolsResp *infopoolsresp =
                    command->mutable_infopoolsresp();

            for (std::shared_ptr<OpenLTFSCartridge> c : pool->getCartridges()) {
                numCartridges++;
                total += c->get_total_cap();
                free += c->get_remaining_cap();
                // unref?
            }

            infopoolsresp->set_poolname(pool->getPoolName());
            infopoolsresp->set_total(total);
            infopoolsresp->set_free(free);
            infopoolsresp->set_unref(unref);
            infopoolsresp->set_numtapes(numCartridges);

            try {
                command->send();
            } catch (const std::exception& e) {
                TRACE(Trace::error, e.what());
                MSG(LTFSDMS0007E);
            }
        }
    }

    LTFSDmProtocol::LTFSDmInfoPoolsResp *infopoolsresp =
            command->mutable_infopoolsresp();

    infopoolsresp->set_poolname("");
    infopoolsresp->set_total(0);
    infopoolsresp->set_free(0);
    infopoolsresp->set_unref(0);
    infopoolsresp->set_numtapes(0);

    try {
        command->send();
    } catch (const std::exception& e) {
        TRACE(Trace::error, e.what());
        MSG(LTFSDMS0007E);
    }
}

void MessageParser::retrieveMessage(long key, LTFSDmCommServer *command)

{
    TRACE(Trace::always, __PRETTY_FUNCTION__);
    const LTFSDmProtocol::LTFSDmRetrieveRequest retrievereq =
            command->retrieverequest();
    long keySent = retrievereq.key();
    int error = Error::LTFSDM_OK;

    TRACE(Trace::normal, keySent);

    if (key != keySent) {
        MSG(LTFSDMS0008E, keySent);
        return;
    }

    try {
        inventory->inventorize();
    } catch (const OpenLTFSException& e) {
        error = e.getError();
    } catch (const std::exception& e) {
        error = Const::UNSET;
    }

    LTFSDmProtocol::LTFSDmRetrieveResp *retrieveresp =
            command->mutable_retrieveresp();

    retrieveresp->set_error(error);

    try {
        command->send();
    } catch (const std::exception& e) {
        TRACE(Trace::error, e.what());
        MSG(LTFSDMS0007E);
    }
}

void MessageParser::run(long key, LTFSDmCommServer command,
        Connector *connector)

{
    TRACE(Trace::always, __PRETTY_FUNCTION__);

    std::unique_lock<std::mutex> lock(Server::termmtx);
    bool firstTime = true;
    long localReqNumber = Const::UNSET;

    while (true) {
        try {
            command.recv();
        } catch (const std::exception& e) {
            TRACE(Trace::error, e.what());
            MSG(LTFSDMS0006E);
            Server::termcond.notify_one();
            lock.unlock();
            return;
        }

        TRACE(Trace::full, "new message received");

        if (command.has_reqnum()) {
            requestNumber(key, &command, &localReqNumber);
        } else {
            if (command.has_stoprequest()) {
                stopMessage(key, &command, &lock, localReqNumber);
            } else {
                if (firstTime) {
                    Server::termcond.notify_one();
                    lock.unlock();
                    firstTime = false;
                }
                if (command.has_migrequest()) {
                    migrationMessage(key, &command, localReqNumber);
                } else if (command.has_selrecrequest()) {
                    selRecallMessage(key, &command, localReqNumber);
                } else if (command.has_statusrequest()) {
                    statusMessage(key, &command, localReqNumber);
                } else if (command.has_addrequest()) {
                    addMessage(key, &command, localReqNumber, connector);
                } else if (command.has_inforequestsrequest()) {
                    infoRequestsMessage(key, &command, localReqNumber);
                } else if (command.has_infojobsrequest()) {
                    infoJobsMessage(key, &command, localReqNumber);
                } else if (command.has_infodrivesrequest()) {
                    infoDrivesMessage(key, &command);
                } else if (command.has_infotapesrequest()) {
                    infoTapesMessage(key, &command);
                } else if (command.has_poolcreaterequest()) {
                    poolCreateMessage(key, &command);
                } else if (command.has_pooldeleterequest()) {
                    poolDeleteMessage(key, &command);
                } else if (command.has_pooladdrequest()) {
                    poolAddMessage(key, &command);
                } else if (command.has_poolremoverequest()) {
                    poolRemoveMessage(key, &command);
                } else if (command.has_infopoolsrequest()) {
                    infoPoolsMessage(key, &command);
                } else if (command.has_retrieverequest()) {
                    retrieveMessage(key, &command);
                } else {
                    TRACE(Trace::error, "unkown command\n");
                }
            }
            break;
        }
    }
    command.closeAcc();
}
