#include "ServerIncludes.h"

void TransRecall::addRequest(Connector::rec_info_t recinfo, std::string tapeId,
        long reqNum)

{
    struct stat statbuf;
    SQLStatement stmt;
    std::string tapeName;
    int state;
    FsObj::mig_attr_t attr;
    std::string filename;
    bool reqExists = false;

    if (recinfo.filename.compare("") == 0)
        filename = "NULL";
    else
        filename = std::string("'") + recinfo.filename + "'";

    try {
        FsObj fso(recinfo);
        statbuf = fso.stat();

        if (!S_ISREG(statbuf.st_mode)) {
            MSG(LTFSDMS0032E, recinfo.ino);
            return;
        }

        state = fso.getMigState();

        if (state == FsObj::RESIDENT) {
            MSG(LTFSDMS0031I, recinfo.ino);
            return;
        }

        attr = fso.getAttribute();

        tapeName = Server::getTapeName(recinfo.fsid, recinfo.igen, recinfo.ino,
                tapeId);
    } catch (const std::exception& e) {
        TRACE(Trace::error, e.what());
        if (filename.compare("NULL") != 0)
            MSG(LTFSDMS0073E, filename);
        else
            MSG(LTFSDMS0032E, recinfo.ino);
    }

    stmt(TransRecall::ADD_JOB) << DataBase::TRARECALL << filename.c_str()
            << reqNum
            << (recinfo.toresident ? FsObj::RESIDENT : FsObj::PREMIGRATED)
            << Const::UNSET << statbuf.st_size << (long long) recinfo.fsid
            << recinfo.igen << (long long) recinfo.ino << statbuf.st_mtime << 0
            << time(NULL) << state << tapeId << Server::getStartBlock(tapeName)
            << (std::intptr_t) recinfo.conn_info;

    TRACE(Trace::normal, stmt.str());

    stmt.doall();

    if (filename.compare("NULL") != 0)
        TRACE(Trace::always, filename);
    else
        TRACE(Trace::always, recinfo.ino);

    TRACE(Trace::always, tapeId);

    std::unique_lock<std::mutex> lock(Scheduler::mtx);

    stmt(TransRecall::CHECK_REQUEST_EXISTS) << reqNum;
    stmt.prepare();
    while (stmt.step())
        reqExists = true;
    stmt.finalize();

    if (reqExists == true) {
        stmt(TransRecall::CHANGE_REQUEST_TO_NEW) << DataBase::REQ_NEW << reqNum
                << tapeId;
        TRACE(Trace::normal, stmt.str());
        stmt.doall();
        Scheduler::cond.notify_one();
    } else {
        stmt(TransRecall::ADD_REQUEST) << DataBase::TRARECALL << reqNum
                << attr.tapeId[0] << time(NULL) << DataBase::REQ_NEW;
        TRACE(Trace::normal, stmt.str());
        stmt.doall();
        Scheduler::cond.notify_one();
    }
}

void TransRecall::cleanupEvents()

{
    Connector::rec_info_t recinfo;
    SQLStatement stmt = SQLStatement(TransRecall::REMAINING_JOBS)
            << DataBase::TRARECALL;
    TRACE(Trace::normal, stmt.str());
    stmt.prepare();
    while (stmt.step(&recinfo.fsid, &recinfo.igen, &recinfo.ino,
            &recinfo.filename, (std::intptr_t *) &recinfo.conn_info)) {
        TRACE(Trace::always, recinfo.filename, recinfo.ino);
        Connector::respondRecallEvent(recinfo, false);
    }
    stmt.finalize();
}

void TransRecall::run(Connector *connector)

{
    ThreadPool<TransRecall, Connector::rec_info_t, std::string, long> wq(
            &TransRecall::addRequest, Const::MAX_TRANSPARENT_RECALL_THREADS,
            "trec-wq");
    Connector::rec_info_t recinfo;
    std::map<std::string, long> reqmap;
    std::set<std::string> fsList;
    std::set<std::string>::iterator it;
    std::string tapeId;

    try {
        connector->initTransRecalls();
    } catch (const std::exception& e) {
        TRACE(Trace::error, e.what());
        MSG(LTFSDMS0030E);
        return;
    }

    fsList = LTFSDM::getFs();

    for (it = fsList.begin(); it != fsList.end(); ++it) {
        try {
            FsObj fileSystem(*it);
            if (fileSystem.isFsManaged()) {
                MSG(LTFSDMS0042I, *it);
                fileSystem.manageFs(true, connector->getStartTime());
            }
        } catch (const OpenLTFSException& e) {
            TRACE(Trace::error, e.what());
            switch (e.getError()) {
                case Error::LTFSDM_FS_CHECK_ERROR:
                    MSG(LTFSDMS0044E, *it);
                    break;
                case Error::LTFSDM_FS_ADD_ERROR:
                    MSG(LTFSDMS0045E, *it);
                    break;
                default:
                    MSG(LTFSDMS0045E, *it);
            }
        } catch (const std::exception& e) {
            TRACE(Trace::error, e.what());
        }
    }

    while (Connector::connectorTerminate == false) {
        try {
            recinfo = connector->getEvents();
        } catch (const std::exception& e) {
            MSG(LTFSDMS0036W, e.what());
        }

        // is sent for termination
        if (recinfo.conn_info == NULL) {
            TRACE(Trace::always, recinfo.ino);
            continue;
        }

        if (Server::terminate == true) {
            TRACE(Trace::always, (bool) Server::terminate);
            connector->respondRecallEvent(recinfo, false);
            continue;
        }

        if (recinfo.ino == 0) {
            TRACE(Trace::always, recinfo.ino);
            continue;
        }

        // error case: managed region set but no attrs
        try {
            FsObj fso(recinfo);

            if (fso.getMigState() == FsObj::RESIDENT) {
                fso.finishRecall(FsObj::RESIDENT);
                MSG(LTFSDMS0039I, recinfo.ino);
                connector->respondRecallEvent(recinfo, true);
                continue;
            }

            tapeId = fso.getAttribute().tapeId[0];
        } catch (const OpenLTFSException& e) {
            TRACE(Trace::error, e.what());
            if (e.getError() == Error::LTFSDM_ATTR_FORMAT)
                MSG(LTFSDMS0037W, recinfo.ino);
            else
                MSG(LTFSDMS0038W, recinfo.ino, e.getError());
            connector->respondRecallEvent(recinfo, false);
            continue;
        } catch (const std::exception& e) {
            TRACE(Trace::error, e.what());
            connector->respondRecallEvent(recinfo, false);
            continue;
        }

        std::stringstream thrdinfo;
        thrdinfo << "TrRec(" << recinfo.ino << ")";

        if (reqmap.count(tapeId) == 0)
            reqmap[tapeId] = ++globalReqNumber;

        TRACE(Trace::always, recinfo.ino, tapeId, reqmap[tapeId]);

        wq.enqueue(Const::UNSET, TransRecall(), recinfo, tapeId,
                reqmap[tapeId]);
        /*
         subs.enqueue(thrdinfo.str(), TransRecall::addRequest, recinfo, tapeId,
         reqmap[tapeId]);
         */
    }

    MSG(LTFSDMS0083I);
    connector->endTransRecalls();
    wq.waitCompletion(Const::UNSET);
    cleanupEvents();
    MSG(LTFSDMS0084I);
}

unsigned long TransRecall::recall(Connector::rec_info_t recinfo,
        std::string tapeId, FsObj::file_state state, FsObj::file_state toState)

{
    struct stat statbuf;
    std::string tapeName;
    char buffer[Const::READ_BUFFER_SIZE];
    long rsize;
    long wsize;
    int fd = -1;
    long offset = 0;
    FsObj::file_state curstate;

    try {
        FsObj target(recinfo);

        TRACE(Trace::always, recinfo.ino, recinfo.filename);

        std::lock_guard<FsObj> fsolock(target);

        curstate = target.getMigState();

        if (curstate != state) {
            MSG(LTFSDMS0034I, recinfo.ino);
            state = curstate;
        }
        if (state == FsObj::RESIDENT) {
            return 0;
        } else if (state == FsObj::MIGRATED) {
            tapeName = Server::getTapeName(recinfo.fsid, recinfo.igen,
                    recinfo.ino, tapeId);
            fd = open(tapeName.c_str(), O_RDWR);

            if (fd == -1) {
                TRACE(Trace::error, errno);
                MSG(LTFSDMS0021E, tapeName.c_str());
                THROW(Const::UNSET, tapeName, errno);
            }

            statbuf = target.stat();

            target.prepareRecall();

            while (offset < statbuf.st_size) {
                if (Server::forcedTerminate)
                    THROW(Const::UNSET, tapeName);

                rsize = read(fd, buffer, sizeof(buffer));
                if (rsize == -1) {
                    TRACE(Trace::error, errno);
                    MSG(LTFSDMS0023E, tapeName.c_str());
                    THROW(Const::UNSET, tapeName, errno);
                }
                wsize = target.write(offset, (unsigned long) rsize, buffer);
                if (wsize != rsize) {
                    TRACE(Trace::error, errno, wsize, rsize);
                    MSG(LTFSDMS0033E, recinfo.ino);
                    close(fd);
                    THROW(Const::UNSET, recinfo.ino, wsize, rsize);
                }
                offset += rsize;
            }

            close(fd);
        }

        target.finishRecall(toState);
        if (toState == FsObj::RESIDENT)
            target.remAttribute();
    } catch (const std::exception& e) {
        TRACE(Trace::error, e.what());
        if (fd != -1)
            close(fd);
        THROW(Const::UNSET);
    }

    return statbuf.st_size;
}

void TransRecall::processFiles(int reqNum, std::string tapeId)

{
    Connector::rec_info_t recinfo;
    SQLStatement stmt;
    FsObj::file_state state;
    FsObj::file_state toState;
    struct respinfo_t
    {
        Connector::rec_info_t recinfo;bool succeeded;
    };
    std::list<respinfo_t> resplist;
    int numFiles = 0;
    bool succeeded;

    stmt(TransRecall::SET_RECALLING) << FsObj::RECALLING_MIG << reqNum
            << FsObj::MIGRATED << tapeId;
    TRACE(Trace::normal, stmt.str());
    stmt.doall();

    stmt(TransRecall::SET_RECALLING) << FsObj::RECALLING_PREMIG << reqNum
            << FsObj::PREMIGRATED << tapeId;
    TRACE(Trace::normal, stmt.str());
    stmt.doall();

    stmt(TransRecall::SELECT_JOBS) << reqNum << FsObj::RECALLING_MIG
            << FsObj::RECALLING_PREMIG << tapeId;
    TRACE(Trace::normal, stmt.str());
    stmt.prepare();
    while (stmt.step(&recinfo.fsid, &recinfo.igen, &recinfo.ino,
            &recinfo.filename, &state, &toState,
            (std::intptr_t *) &recinfo.conn_info)) {
        numFiles++;

        if (state == FsObj::RECALLING_MIG)
            state = FsObj::MIGRATED;
        else
            state = FsObj::PREMIGRATED;

        if (toState == FsObj::RESIDENT)
            recinfo.toresident = true;

        TRACE(Trace::always, recinfo.filename, recinfo.ino, state, toState);

        try {
            recall(recinfo, tapeId, state, toState);
            succeeded = true;
        } catch (const std::exception& e) {
            TRACE(Trace::error, e.what());
            succeeded = false;
        }

        TRACE(Trace::always, succeeded);
        resplist.push_back((respinfo_t ) { recinfo, succeeded });
    }
    stmt.finalize();
    TRACE(Trace::always, numFiles);

    stmt(TransRecall::DELETE_JOBS) << reqNum << FsObj::RECALLING_MIG
            << FsObj::RECALLING_PREMIG << tapeId;
    TRACE(Trace::normal, stmt.str());
    stmt.doall();

    for (respinfo_t respinfo : resplist)
        Connector::respondRecallEvent(respinfo.recinfo, respinfo.succeeded);
}

void TransRecall::execRequest(int reqNum, std::string tapeId)

{
    SQLStatement stmt;
    int remaining = 0;

    TRACE(Trace::always, reqNum, tapeId);

    processFiles(reqNum, tapeId);

    std::unique_lock<std::mutex> lock(Scheduler::mtx);

    {
        std::lock_guard<std::recursive_mutex> inventorylock(
                OpenLTFSInventory::mtx);
        inventory->getCartridge(tapeId)->setState(OpenLTFSCartridge::MOUNTED);
        bool found = false;
        for (std::shared_ptr<OpenLTFSDrive> d : inventory->getDrives()) {
            if (d->get_slot() == inventory->getCartridge(tapeId)->get_slot()) {
                TRACE(Trace::normal, d->GetObjectID());
                d->setFree();
                found = true;
                break;
            }
        }
        assert(found == true);
    }

    stmt(TransRecall::COUNT_REMAINING_JOBS) << reqNum << tapeId;
    TRACE(Trace::normal, stmt.str());
    stmt.prepare();
    while (stmt.step(&remaining)) {
    }
    stmt.finalize();

    if (remaining)
        stmt(TransRecall::CHANGE_REQUEST_TO_NEW) << DataBase::REQ_NEW << reqNum
                << tapeId;
    else
        stmt(TransRecall::DELETE_REQUEST) << reqNum << tapeId;
    TRACE(Trace::normal, stmt.str());
    stmt.doall();
    Scheduler::cond.notify_one();
}
