/*******************************************************************************
 * Copyright 2018 IBM Corp. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *  https://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 *******************************************************************************/
#include "ServerIncludes.h"

/**
    @page message_parsing Message parsing

    The message parsing is invoked by Receiver calling the MessageParser::run
    method. Within this method it is determined which Protocol
    Buffer message has been received. The Protocol Buffers framework
    provides functions to determine which command has been issued
    on the client side. If the migration command has been
    called the following method

    command.has_migrequest()

    should be used to identify. This identification is part of the
    MessageParser::run method. Further parsing and processing
    is performed within command specific methods:

    Message | Description
    ---|---
    MessageParser::reqStatusMessage | query results after a migration or recall request have been sent
    MessageParser::migrationMessage | migration command
    MessageParser::selRecallMessage | recall command
    MessageParser::requestNumber | message to get a request number
    MessageParser::stopMessage | stop command
    MessageParser::statusMessage | status command
    MessageParser::addMessage | add command
    MessageParser::infoRequestsMessage | info requests command
    MessageParser::infoJobsMessage | info jobs command
    MessageParser::infoDrivesMessage | info drives command
    MessageParser::infoTapesMessage | info tapes command
    MessageParser::poolCreateMessage | pool create command
    MessageParser::poolDeleteMessage | pool delete command
    MessageParser::poolAddMessage | pool add command
    MessageParser::poolRemoveMessage | pool remove command
    MessageParser::infoPoolsMessage | info pools command
    MessageParser::retrieveMessage | retrieve command

    For selective recall and migration the file names need to be transferred
    from the client to the backend. This is handled within the MessageParser::getObjects
    method. Sending the objects and querying the migration and recall status
    is performed over same connection like the initial migration and recall
    requests to be followed and not processed by the Receiver.

    The following graph provides an overview of the complete client message processing:

    @dot
    digraph message_processing {
        compound=yes;
        fontname="courier";
        fontsize=11;
        labeljust=l;
        node [shape=record, width=1, fontname="courier", fontsize=11, fillcolor=white, style=filled];
        listen [fontname="courier bold", fontcolor=dodgerblue4, label="command.listen", URL="@ref LTFSDmCommServer::listen"];
        subgraph cluster_loop {
            label="while not terminated"
            accept [fontname="courier bold", fontcolor=dodgerblue4, label="command.accept", URL="@ref LTFSDmCommServer::accept"];
            run [fontname="courier bold", fontcolor=dodgerblue4, label="MessageParser::run", URL="@ref MessageParser::run"];
            msg [label="MessageParser::...Message"];
            mig_msg [fontname="courier bold", fontcolor=dodgerblue4, label="MessageParser::migrationMessage", URL="@ref MessageParser::migrationMessage"];
            rec_msg [fontname="courier bold", fontcolor=dodgerblue4, label="MessageParser::selRecallMessage", URL="@ref MessageParser::selRecallMessage"];
            get_objects [fontname="courier bold", fontcolor=dodgerblue4, label="MessageParser::getObjects", URL="@ref MessageParser::getObjects"];
            req_status_msg [fontname="courier bold", fontcolor=dodgerblue4, label="MessageParser::reqStatusMessage", URL="@ref MessageParser::reqStatusMessage"];
        }
        listen -> accept [lhead=cluster_loop, minlen=2];
        accept -> run [fontname="courier bold", fontsize=8, fontcolor=dodgerblue4, label="wqm.enqueue", URL="@ref ThreadPool::enqueue"];
        run -> msg [];
        run -> mig_msg [];
        run -> rec_msg [];
        mig_msg -> get_objects [fontsize=8, label="1st"];
        mig_msg -> req_status_msg [fontsize=8, label="2nd"];
        rec_msg -> get_objects [fontsize=8, label="1st"];
        rec_msg -> req_status_msg [fontsize=8, label="2nd"];
    }
    @enddot


 */

void MessageParser::getObjects(LTFSDmCommServer *command, long localReqNumber,
        unsigned long pid, long requestNumber, FileOperation *fopt,
        std::set<std::string> pools = { })

{
    bool cont = true;
    int error = static_cast<int>(Error::OK);

    TRACE(Trace::full, __PRETTY_FUNCTION__);

    while (cont) {
        if (Server::forcedTerminate)
            return;

        try {
            command->recv();
        } catch (const std::exception& e) {
            TRACE(Trace::error, e.what());
            MSG(LTFSDMS0006E);
            THROW(Error::GENERAL_ERROR);
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
                } catch (const LTFSDMException& e) {
                    TRACE(Trace::error, e.what());
                    if (e.getErrno() == SQLITE_CONSTRAINT_PRIMARYKEY
                            || e.getErrno() == SQLITE_CONSTRAINT_UNIQUE)
                        MSG(LTFSDMS0019E, filename.filename().c_str());
                    else
                        MSG(LTFSDMS0015E, filename.filename().c_str(),
                                e.what());
                } catch (const std::exception& e) {
                    TRACE(Trace::error, e.what());
                }
            } else {
                cont = false; // END
            }
        }

        if (cont == false) {
            for (std::string pool : pools) {
                unsigned long free = 0;
                for (std::string cartridgeid : Server::conf.getPool(pool)) {
                    std::shared_ptr<LTFSDMCartridge> cart =
                            inventory->getCartridge(cartridgeid);
                    if (cart != nullptr)
                        free += cart->get_le()->get_remaining_cap();
                }
                free *= (1024*1024);
                if (fopt->getRequestSize() > free) {
                    TRACE(Trace::always, fopt->getRequestSize(), free);
                    error = static_cast<int>(Error::POOL_TOO_SMALL);
                }
            }
        }

        LTFSDmProtocol::LTFSDmSendObjectsResp *sendobjresp =
                command->mutable_sendobjectsresp();

        sendobjresp->set_error(error);
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
    long transferred = 0;
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

        done = fopt->queryResult(requestNumber, &resident, &transferred,
                &premigrated, &migrated, &failed);

        LTFSDmProtocol::LTFSDmReqStatusResp *reqstatusresp =
                command->mutable_reqstatusresp();

        reqstatusresp->set_success(true);
        reqstatusresp->set_reqnumber(requestNumber);
        reqstatusresp->set_pid(pid);
        reqstatusresp->set_resident(resident);
        reqstatusresp->set_transferred(transferred);
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
    int error = static_cast<int>(Error::OK);
    Migration *mig = nullptr;
    std::set<std::string> allpools;

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
            std::lock_guard<std::recursive_mutex> lock(LTFSDMInventory::mtx);
            while (std::getline(poolss, pool, ',')) {
                allpools = Server::conf.getPools();
                if (allpools.find(pool) == allpools.end()) {
                    error = static_cast<int>(Error::NOT_ALL_POOLS_EXIST);
                    break;
                }
                if (pools.count(pool) == 0)
                    pools.insert(pool);
            }
        }

        if (!error && (pools.size() > 3)) {
            error = static_cast<int>(Error::WRONG_POOLNUM);
        }

        mig = new Migration(pid, requestNumber, pools, pools.size(),
                migreq.state());
    } else {
        error = static_cast<int>(Error::TERMINATING);
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
                    dynamic_cast<FileOperation*>(mig), pools);
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
    int error = static_cast<int>(Error::OK);
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
        error = static_cast<int>(Error::TERMINATING);

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

    Scheduler::invoke();

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
        long localReqNumber, std::shared_ptr<Connector> connector)

{
    TRACE(Trace::always, __PRETTY_FUNCTION__);
    const LTFSDmProtocol::LTFSDmAddRequest addreq = command->addrequest();
    long keySent = addreq.key();
    std::string managedFs = addreq.managedfs();
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
            fileSystem.manageFs(true, connector->getStartTime());
        }
    } catch (const LTFSDMException& e) {
        response = LTFSDmProtocol::LTFSDmAddResp::FAILED;
        TRACE(Trace::error, e.what());
        switch (e.getError()) {
            case Error::FS_CHECK_ERROR:
                MSG(LTFSDMS0044E, managedFs);
                break;
            case Error::FS_ADD_ERROR:
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
        inforeqsresp->set_targetstate(FsObj::migStateStr(tgtstate));
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
        infojobsresp->set_state(state);

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
    infojobsresp->set_state(Const::UNSET);

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
        std::lock_guard<std::recursive_mutex> lock(LTFSDMInventory::mtx);
        for (std::shared_ptr<LTFSDMDrive> d : inventory->getDrives()) {
            LTFSDmProtocol::LTFSDmInfoDrivesResp *infodrivesresp =
                    command->mutable_infodrivesresp();

            infodrivesresp->set_id(d->get_le()->GetObjectID());
            infodrivesresp->set_devname(d->get_le()->get_devname());
            infodrivesresp->set_slot(d->get_le()->get_slot());
            infodrivesresp->set_status(d->get_le()->get_status());
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
        std::lock_guard<std::recursive_mutex> lock(LTFSDMInventory::mtx);
        for (std::shared_ptr<LTFSDMCartridge> c : inventory->getCartridges()) {
            LTFSDmProtocol::LTFSDmInfoTapesResp *infotapesresp =
                    command->mutable_infotapesresp();

            infotapesresp->set_id(c->get_le()->GetObjectID());
            infotapesresp->set_slot(c->get_le()->get_slot());
            infotapesresp->set_totalcap(c->get_le()->get_total_cap()/1024);
            infotapesresp->set_remaincap(c->get_le()->get_remaining_cap()/1024);
            // the size of the reclaimable space is an estimation
            infotapesresp->set_reclaimable(
                    (c->get_le()->get_total_blocks()
                            - c->get_le()->get_valid_blocks())
                            * inventory->getBlockSize() / (1024 * 1024 * 1024));
            infotapesresp->set_status(c->get_le()->get_status());
            infotapesresp->set_inprogress(c->getInProgress());
            infotapesresp->set_pool(c->getPool());
            switch (c->getState()) {
                case LTFSDMCartridge::TAPE_INUSE:
                    state = ltfsdm_messages[LTFSDMS0055I];
                    break;
                case LTFSDMCartridge::TAPE_MOUNTED:
                    state = ltfsdm_messages[LTFSDMS0056I];
                    break;
                case LTFSDMCartridge::TAPE_MOVING:
                    state = ltfsdm_messages[LTFSDMS0057I];
                    break;
                case LTFSDMCartridge::TAPE_UNMOUNTED:
                    state = ltfsdm_messages[LTFSDMS0058I];
                    break;
                case LTFSDMCartridge::TAPE_INVALID:
                    state = ltfsdm_messages[LTFSDMS0059I];
                    break;
                case LTFSDMCartridge::TAPE_UNKNOWN:
                    state = ltfsdm_messages[LTFSDMS0060I];
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
    infotapesresp->set_reclaimable(0);
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
    int response = static_cast<int>(Error::OK);

    TRACE(Trace::normal, keySent);

    if (key != keySent) {
        MSG(LTFSDMS0008E, keySent);
        return;
    }

    poolName = poolcreate.poolname();

    {
        std::lock_guard<std::recursive_mutex> lock(LTFSDMInventory::mtx);
        try {
            inventory->poolCreate(poolName);
        } catch (const LTFSDMException& e) {
            response = static_cast<int>(e.getError());
        } catch (const std::exception& e) {
            response = static_cast<int>(Error::GENERAL_ERROR);
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
    int response = static_cast<int>(Error::OK);

    TRACE(Trace::normal, keySent);

    if (key != keySent) {
        MSG(LTFSDMS0008E, keySent);
        return;
    }

    poolName = pooldelete.poolname();

    {
        std::lock_guard<std::recursive_mutex> lock(LTFSDMInventory::mtx);
        try {
            inventory->poolDelete(poolName);
        } catch (const LTFSDMException& e) {
            response = static_cast<int>(e.getError());
        } catch (const std::exception& e) {
            response = static_cast<int>(Error::GENERAL_ERROR);
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
    bool format;
    bool check;
    bool wait;
    std::string poolName;
    std::list<std::string> tapeids;
    std::shared_ptr<LTFSDMCartridge> cartridge;
    std::string tapeStatus;
    int response;

    TRACE(Trace::normal, keySent);

    if (key != keySent) {
        MSG(LTFSDMS0008E, keySent);
        return;
    }

    format = pooladd.format();
    check = pooladd.check();
    poolName = pooladd.poolname();

    try {
        Server::conf.getPool(poolName);
    } catch (const LTFSDMException& e) {
        MSG(LTFSDMX0025E, poolName);
        response = static_cast<int>(e.getError());
        LTFSDmProtocol::LTFSDmPoolResp *poolresp = command->mutable_poolresp();
        poolresp->set_tapeid("");
        poolresp->set_response(response);

        try {
            command->send();
        } catch (const std::exception& e) {
            TRACE(Trace::error, e.what());
            MSG(LTFSDMS0007E);
        }
    }

    for (int i = 0; i < pooladd.tapeid_size(); i++)
        tapeids.push_back(pooladd.tapeid(i));

    for (std::string tapeid : tapeids) {
        response = static_cast<int>(Error::OK);
        wait = false;

        try {
            std::unique_lock<std::recursive_mutex> lock(LTFSDMInventory::mtx);

            if ((cartridge = inventory->getCartridge(tapeid)) == nullptr) {
                MSG(LTFSDMX0034E, tapeid);
                THROW(Error::TAPE_NOT_EXISTS);
            }

            if (cartridge->getPool().compare("") != 0) {
                MSG(LTFSDMX0021E, tapeid);
                THROW(Error::TAPE_EXISTS_IN_POOL);
            }

            try {
                tapeStatus = cartridge->get_le()->get_status();
            } catch (const std::exception& e) {
                MSG(LTFSDMX0086E, tapeid);
                THROW(Error::UNKNOWN_FORMAT_STATUS);
            }

            if (format) {
                if (tapeStatus.compare("WRITABLE") != 0) {
                    MSG(LTFSDMC0106I, tapeid);
                    TapeHandler th(poolName, tapeid, TapeHandler::FORMAT);
                    th.addRequest();
                    wait = true;
                } else {
                    MSG(LTFSDMX0083E, tapeid);
                    THROW(Error::ALREADY_FORMATTED);
                }
            } else if (check) {
                if (tapeStatus.compare("NOT_MOUNTED_YET") == 0
                        || tapeStatus.compare("WRITABLE") == 0) {
                    MSG(LTFSDMC0107I, tapeid);
                    TapeHandler th(poolName, tapeid, TapeHandler::CHECK);
                    th.addRequest();
                    wait = true;
                } else {
                    MSG(LTFSDMX0084E, tapeid);
                    response = static_cast<int>(Error::NOT_FORMATTED);
                    THROW(Error::NOT_FORMATTED);
                }
            } else {
                if (tapeStatus.compare("WRITABLE") == 0) {
                    try {
                        inventory->poolAdd(poolName, tapeid);
                    } catch (const LTFSDMException& e) {
                        response = static_cast<int>(e.getError());
                    } catch (const std::exception& e) {
                        THROW(Error::GENERAL_ERROR);
                    }
                } else {
                    MSG(LTFSDMS0110E, tapeid, tapeStatus);
                    THROW(Error::TAPE_NOT_WRITABLE);
                }
            }
        } catch (const LTFSDMException& e) {
            response = static_cast<int>(e.getError());
        } catch (const std::exception& e) {
            response = static_cast<int>(Error::GENERAL_ERROR);
        }

        if (wait) {
            if ((cartridge = inventory->getCartridge(tapeid)) == nullptr) {
                MSG(LTFSDMX0034E, tapeid);
                response = static_cast<int>(Error::TAPE_NOT_EXISTS);
                break;
            }
            std::unique_lock<std::mutex> lock(cartridge->mtx);
            Scheduler::invoke();
            cartridge->cond.wait(lock);
            response = static_cast<int>(cartridge->result);
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

    Scheduler::invoke();
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
        response = static_cast<int>(Error::OK);

        {
            std::lock_guard<std::recursive_mutex> lock(LTFSDMInventory::mtx);
            try {
                inventory->poolRemove(poolName, tapeid);
            } catch (const LTFSDMException& e) {
                response = static_cast<int>(e.getError());
            } catch (const std::exception& e) {
                response = static_cast<int>(Error::GENERAL_ERROR);
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
    std::shared_ptr<LTFSDMCartridge> c;
    std::string state;

    TRACE(Trace::normal, keySent);

    if (key != keySent) {
        MSG(LTFSDMS0008E, keySent);
        return;
    }

    {
        std::lock_guard<std::recursive_mutex> lock(LTFSDMInventory::mtx);
        for (std::string poolname : Server::conf.getPools()) {
            int numCartridges = 0;
            unsigned long total = 0;
            unsigned long free = 0;
            unsigned long unref = 0;

            LTFSDmProtocol::LTFSDmInfoPoolsResp *infopoolsresp =
                    command->mutable_infopoolsresp();

            for (std::string cartridgeid : Server::conf.getPool(poolname)) {
                if ((c = inventory->getCartridge(cartridgeid)) == nullptr) {
                    MSG(LTFSDMX0034E, cartridgeid);
                    Server::conf.poolRemove(poolname, cartridgeid);
                } else {
                    numCartridges++;
                    total += c->get_le()->get_total_cap();
                    free += c->get_le()->get_remaining_cap();
                }
                // unref?
            }

            infopoolsresp->set_poolname(poolname);
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
    int error = static_cast<int>(Error::OK);

    TRACE(Trace::normal, keySent);

    if (key != keySent) {
        MSG(LTFSDMS0008E, keySent);
        return;
    }

    try {
        inventory->inventorize();
    } catch (const LTFSDMException& e) {
        MSG(LTFSDMS0101E, e.what());
        error = static_cast<int>(e.getError());
    } catch (const std::exception& e) {
        error = static_cast<int>(Error::GENERAL_ERROR);
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
        std::shared_ptr<Connector> connector)

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
