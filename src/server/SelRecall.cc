#include "ServerIncludes.h"

/** @page selective_recall Selective Recall

    # SelRecall

    The selective recall processing happens within two phases:

    1. The backend receives a selective recall message which is
       further processed within the MessageParser::selRecallMessage
       method. During this processing selective recall jobs and selective
       recall requests are added to the internal queues.
    2. The Scheduler identifies a selective recall request to get scheduled.
       The order of files being recalled depends on the starting block of
       the data files on tape: @snippet server/SQLStatements.cc sel_recall_sql_qry

    @dot
    digraph sel_recall {
        compound=true;
        fontname="fixed";
        fontsize=11;
        labeljust=l;
        node [shape=record, width=2, fontname="fixed", fontsize=11, fillcolor=white, style=filled];
        subgraph cluster_first {
            label="first phase";
            recv [label="Receiver"];
            msgparser [label="MessageParser"];
            recv -> msgparser [];
        }
        subgraph cluster_second {
            label="second phase";
            scheduler [label="Scheduler"];
            subgraph cluster_rec_exec {
                label="SelRecall::execRequest";
                rec_exec [label="read from tape\n(SelRecall::processFiles)\nordered by starting block"];
            }
            scheduler -> rec_exec [label="schedule\nrecall request", fontname="fixed", fontsize=8, lhead=cluster_rec_exec];
        }
        subgraph cluster_tables {
            label="SQLite tables";
            tables [label="<rq> REQUEST_QUEUE|<jq> JOB_QUEUE"];
        }
        msgparser -> scheduler [color=blue, fontcolor=blue, label="condition", fontname="fixed", fontsize=8];
        scheduler -> msgparser [style=invis]; // just for the correct order of the subgraphs
        msgparser -> tables [color=darkgreen, fontcolor=darkgreen, label="add", fontname="fixed", fontsize=8, headport=w, lhead=cluster_tables];
        scheduler -> tables:rq [color=darkgreen, fontcolor=darkgreen, label="check for requests to schedule", fontname="fixed", fontsize=8, tailport=e];
        rec_exec -> tables [color=darkgreen, fontcolor=darkgreen, label="read and update", fontname="fixed", fontsize=8, lhead=cluster_tables, ltail=cluster_rec_exec];
    }
    @enddot

    This high level description is explained in more detail in the following
    subsections.

    The second step will not start before the first step is completed. For
    the second step the required tape and drive resources need to be
    available: e.g. a corresponding tape cartridge is mounted on a tape drive.
    The second phase may start immediately after the first phase but it also
    can take a longer time depending when a required resource gets available.

    ## 1. adding jobs and requests to the internal tables

    When a client sends a selective recall request to the backend the
    corresponding information is split into two parts. The first part
    contains information that is relevant for the whole request: the
    final recall state. A file can be recalled to resident state or
    to premigrated state.

    Thereafter the file names of the files to be recalled are sent to the backend.
    When receiving this information corresponding entries are added to the SQL
    table JOB_QUEUE. For each file one entry is created. After that an entry is
    added to the SQL table REQUEST_QUEUE.

    This is an example of these two tables in case of selectively recalling
    a few files:

    @verbatim
    sqlite> select * from JOB_QUEUE;
    OPERATION   FILE_NAME               REQ_NUM     TARGET_STATE  REPL_NUM    TAPE_POOL   FILE_SIZE   FS_ID_H               FS_ID_L               I_GEN        I_NUM       MTIME_SEC   MTIME_NSEC  LAST_UPD    TAPE_ID     FILE_STATE  START_BLOCK  CONN_INFO
    ----------  ----------------------  ----------  ------------  ----------  ----------  ----------  --------------------  --------------------  -----------  ----------  ----------  ----------  ----------  ----------  ----------  -----------  ----------
    1           /mnt/lxfs/test2/file.0  2           0                                     32768       -4229860921927319251  -6765444084672883911  -1887576650  787590      1514304375  649398977   1515426071  D01301L5    6           207111
    1           /mnt/lxfs/test2/file.2  2           0                                     32768       -4229860921927319251  -6765444084672883911  -316887448   788450      1514304375  653398953   1515426071  D01301L5    6           206251
    1           /mnt/lxfs/test2/file.4  2           0                                     32768       -4229860921927319251  -6765444084672883911  1380830956   788454      1514304375  656398936   1515426071  D01301L5    6           199882
    sqlite> select * from REQUEST_QUEUE;
    OPERATION   REQ_NUM     TARGET_STATE  NUM_REPL    REPL_NUM    TAPE_POOL   TAPE_ID     TIME_ADDED  STATE
    ----------  ----------  ------------  ----------  ----------  ----------  ----------  ----------  ----------
    1           2           0                                                 D01301L5    1515426071  1
    @endverbatim

    For a description of the columns see @ref sqlite.

    The following is an overview of this initial selective recall processing
    including corresponding links to the code parts:

    <TT>
    - MessageParser::selRecallMessage
        - create a SelRecall object with target state information (premigrated or resident state,
          @ref LTFSDmProtocol::LTFSDmSelRecRequest::state "recreq.state()")
        - respond back to the client with a request number
        - MessageParser::getObjects: retrieving file names to recall
            - SelRecall::addJob: add recall information the the SQLite table JOB_QUEUE
        - SelRecall::addRequest: add a request to the SQLite table REQUEST_QUEUE
        - MessageParser::reqStatusMessage: provide updates to the recall processing to the client

    </TT>

    ## 2. Scheduling selective recall jobs

    After a selective recall request has been added to the REQUEST_QUEUE and
    there is a free tape and drive resource available to schedule this
    selective recall request the following will happen:

    <TT>
    - Scheduler::run
        - if a selective request is ready do be scheduled:
            - update record in request queue to mark it as DataBase::REQ_INPROGRESS
            - SelRecall::execRequest
                - add status: @ref Status::add "mrStatus.add"
                - call SelRecall::processFiles depending the target state SelRecall::targetState
                - if @ref SelRecall::needsTape "needsTape" is true:
                    - release tape for further operations since for stubbing files there is nothing written to tape
                - update record in request queue to mark it as DataBase::REQ_COMPLETED

    </TT>

    In SelRecall::execRequest adding an entry to the @ref mrStatus object is
    necessary for the client that initiated the request to receive progress
    information.

    ### SelRecall::processFiles

    The SelRecall::processFiles method is traversing the JOB_QUEUE table to
    process individual files for selective recall. In general the following
    steps are performed:

    -# All corresponding jobs are changed to FsObj::RECALLING_MIG or FsObj::RECALLING_PREMIG
       depending if it is called for files in migrated or in premigrated state. The following
       example shows this change regarding six files:
       @dot
       digraph step_1 {
            compound=true;
            fontname="fixed";
            fontsize=11;
            rankdir=LR;
            node [shape=record, width=2, fontname="fixed", fontsize=11, fillcolor=white, style=filled];
            before [label="file.1: FsObj::MIGRATED|file.2: FsObj::PREMIGRATED|file.3: FsObj::MIGRATED|file.4: FsObj::PREMIGRATED|file.5: FsObj::MIGRATED|file.6: FsObj::PREMIGRATED"];
            after [label="file.1: FsObj::RECALLING_MIG|file.2: FsObj::RECALLING_PREMIG|file.3: FsObj::RECALLING_MIG|file.4: FsObj::RECALLING_PREMIG|file.5: FsObj::RECALLING_MIG|file.6: FsObj::RECALLING_PREMIG"];
            before -> after [];
       }
       @enddot
    -# Process all these jobs in FsObj::RECALLING_MIG or FsObj::RECALLING_PREMIG state
       which results in the recall of all corresponding files. For all jobs in
       FsObj::RECALLING_PREMIG state there will no data transfer happen.
       The following change indicates that the recall of file file.4 failed:
       @dot
       digraph step_1 {
            compound=true;
            fontname="fixed";
            fontsize=11;
            rankdir=LR;
            node [shape=record, width=2, fontname="fixed", fontsize=11, fillcolor=white, style=filled];
            before [label="file.1: FsObj::RECALLING_MIG|file.2: FsObj::RECALLING_PREMIG|file.3: FsObj::RECALLING_MIG|file.4: FsObj::RECALLING_PREMIG|file.5: FsObj::RECALLING_MIG|file.6: FsObj::RECALLING_PREMIG"];
            after [label="file.1: FsObj::RECALLING_MIG|file.2: FsObj::RECALLING_PREMIG|file.3: FsObj::RECALLING_MIG|file.4: FsObj::FAILED|file.5: FsObj::RECALLING_MIG|file.6: FsObj::RECALLING_PREMIG"];
            before -> after [];
       }
       @enddot
    -# A list is returned containing the inode numbers of these files where
       the previous operation was successful. Change all corresponding jobs
       to FsObj::PREMIGRATED or FsObj::RESIDENT depending of the target
       state. The following changed indicates that recall stopped
       before file file.5 and target state is premigrated:
       @dot
       digraph step_1 {
            compound=true;
            fontname="fixed";
            fontsize=11;
            rankdir=LR;
            node [shape=record, width=2, fontname="fixed", fontsize=11, fillcolor=white, style=filled];
            before [label="file.1: FsObj::RECALLING_MIG|file.2: FsObj::RECALLING_PREMIG|file.3: FsObj::RECALLING_MIG|file.4: FsObj::FAILED|file.5: FsObj::RECALLING_MIG|file.6: FsObj::RECALLING_PREMIG"];
            after [label="file.1: FsObj::PREMIGRATED|file.2: FsObj::PREMIGRATED|file.3: FsObj::PREMIGRATED|file.4: FsObj::FAILED|file.5: FsObj::PREMIGRATING|file.6: FsObj::PREMIGRATING"];
            before -> after [];
       }
       @enddot
    -# The remaining jobs (those where no corresponding inode numbers were
       in the list) have not been processed and need to be changed to the
       original state if these were still in FsObj::RECALLING_MIG or
       FsObj::RECALLING_PREMIG state. Jobs that failed in the second step
       already have been marked as FsObj::FAILED. A reason for remaining
       jobs left over from the second step could be that a request with a
       higher priority (e.g. transparent recall) required the same tape resource.
       This change is shown below for the remaining two files of the example:
       @dot
       digraph step_1 {
            compound=true;
            fontname="fixed";
            fontsize=11;
            rankdir=LR;
            node [shape=record, width=2, fontname="fixed", fontsize=11, fillcolor=white, style=filled];
            before [label="file.1: FsObj::PREMIGRATED|file.2: FsObj::PREMIGRATED|file.3: FsObj::PREMIGRATED|file.4: FsObj::FAILED|file.5: FsObj::PREMIGRATING|file.6: FsObj::PREMIGRATING"];
            after [label="file.1: FsObj::PREMIGRATED|file.2: FsObj::PREMIGRATED|file.3: FsObj::PREMIGRATED|file.4: FsObj::FAILED|file.5: FsObj::MIGRATED|file.6: FsObj::PREMIGRATED"];
            before -> after [];
       }
       @enddot

    In opposite to migration recalls are not performed in parallel.
    For an optimal performance the data should be read serially from
    tape in the order of the starting block of each data file.

    ### SelRecall::recall

    Recalling an individual file is performed according the following steps:

    -# If state is FsObj::MIGRATED data is read in a loop from tape and written to disk.
    -# The attributes on the disk file are updated or removed in the case of target state resident.
 */

void SelRecall::addJob(std::string fileName)

{
    struct stat statbuf;
    SQLStatement stmt;
    std::string tapeName;
    int state;
    FsObj::mig_attr_t attr;
    fuid_t fuid;

    try {
        FsObj fso(fileName);
        stat(fileName.c_str(), &statbuf);

        if (!S_ISREG(statbuf.st_mode)) {
            MSG(LTFSDMS0018E, fileName.c_str());
            return;
        }

        state = fso.getMigState();
        if (state == FsObj::RESIDENT) {
            MSG(LTFSDMS0026I, fileName.c_str());
            return;
        }

        attr = fso.getAttribute();

        if (state == FsObj::MIGRATED) {
            needsTape.insert(attr.tapeId[0]);
        }

        tapeName = Server::getTapeName(&fso, attr.tapeId[0]);

        fuid = fso.getfuid();
        stmt(SelRecall::ADD_JOB) << DataBase::SELRECALL << fileName << reqNumber
                << targetState << statbuf.st_size << fuid.fsid_h << fuid.fsid_l
                << fuid.igen << fuid.inum << statbuf.st_mtim.tv_sec
                << statbuf.st_mtim.tv_nsec << time(NULL) << state
                << attr.tapeId[0] << Server::getStartBlock(tapeName);
    } catch (const std::exception& e) {
        TRACE(Trace::error, e.what());
        stmt(SelRecall::ADD_JOB) << DataBase::SELRECALL << fileName << reqNumber
                << targetState << Const::UNSET << Const::UNSET << Const::UNSET
                << Const::UNSET << Const::UNSET << Const::UNSET << time(NULL)
                << FsObj::FAILED << Const::FAILED_TAPE_ID << 0;
        MSG(LTFSDMS0017E, fileName.c_str());
    }

    TRACE(Trace::normal, stmt.str());

    stmt.doall();

    TRACE(Trace::always, fileName, attr.tapeId[0]);

    return;
}

void SelRecall::addRequest()

{
    SQLStatement gettapesstmt = SQLStatement(SelRecall::GET_TAPES) << reqNumber;
    SQLStatement addreqstmt;
    std::string tapeId;
    int state;
    std::stringstream thrdinfo;
    SubServer subs;

    gettapesstmt.prepare();

    {
        std::lock_guard<std::mutex> updlock(Scheduler::updmtx);
        Scheduler::updReq[reqNumber] = false;
    }

    while (gettapesstmt.step(&tapeId)) {
        std::unique_lock<std::mutex> lock(Scheduler::mtx);

        if (tapeId.compare(Const::FAILED_TAPE_ID) == 0)
            state = DataBase::REQ_COMPLETED;
        else if (needsTape.count(tapeId) > 0)
            state = DataBase::REQ_NEW;
        else
            state = DataBase::REQ_INPROGRESS;

        addreqstmt(SelRecall::ADD_REQUEST) << DataBase::SELRECALL << reqNumber
                << targetState << tapeId << time(NULL) << state;

        TRACE(Trace::normal, addreqstmt.str());

        addreqstmt.doall();

        TRACE(Trace::always, needsTape.count(tapeId), reqNumber, tapeId);

        if (needsTape.count(tapeId) > 0) {
            Scheduler::cond.notify_one();
        } else {
            thrdinfo << "SR(" << reqNumber << ")";
            subs.enqueue(thrdinfo.str(), &SelRecall::execRequest,
                    SelRecall(getpid(), reqNumber, targetState), tapeId, false);
        }
    }

    gettapesstmt.finalize();

    subs.waitAllRemaining();
}

unsigned long SelRecall::recall(std::string fileName, std::string tapeId,
        FsObj::file_state state, FsObj::file_state toState)

{
    struct stat statbuf;
    struct stat statbuf_tape;
    std::string tapeName;
    char buffer[Const::READ_BUFFER_SIZE];
    long rsize;
    long wsize;
    int fd = -1;
    long offset = 0;
    FsObj::file_state curstate;

    try {
        FsObj target(fileName);

        TRACE(Trace::always, fileName);

        std::lock_guard<FsObj> fsolock(target);

        curstate = target.getMigState();

        if (curstate != state) {
            MSG(LTFSDMS0035I, fileName);
            state = curstate;
        }
        if (state == FsObj::RESIDENT) {
            return 0;
        } else if (state == FsObj::MIGRATED) {
            tapeName = Server::getTapeName(&target, tapeId);
            fd = open(tapeName.c_str(), O_RDWR | O_CLOEXEC);

            if (fd == -1) {
                TRACE(Trace::error, errno);
                MSG(LTFSDMS0021E, tapeName.c_str());
                THROW(Error::GENERAL_ERROR, tapeName, errno);
            }

            statbuf = target.stat();

            if (fstat(fd, &statbuf_tape) == 0
                    && statbuf_tape.st_size != statbuf.st_size) {
                MSG(LTFSDMS0097W, fileName, statbuf.st_size,
                        statbuf_tape.st_size);
                statbuf.st_size = statbuf_tape.st_size;
                toState = FsObj::RESIDENT;
            }

            target.prepareRecall();

            while (offset < statbuf.st_size) {
                if (Server::forcedTerminate)
                    THROW(Error::OK);

                rsize = read(fd, buffer, sizeof(buffer));
                if (rsize == 0) {
                    break;
                }

                if (rsize == -1) {
                    TRACE(Trace::error, errno);
                    MSG(LTFSDMS0023E, tapeName.c_str());
                    THROW(Error::GENERAL_ERROR, fileName, errno);
                }
                wsize = target.write(offset, (unsigned long) rsize, buffer);
                if (wsize != rsize) {
                    TRACE(Trace::error, errno, wsize, rsize);
                    MSG(LTFSDMS0027E, fileName.c_str());
                    close(fd);
                    THROW(Error::GENERAL_ERROR, fileName, wsize, rsize);
                }
                offset += rsize;
            }

            close(fd);
        }

        target.finishRecall(toState);
        if (toState == FsObj::RESIDENT)
            target.remAttribute();
    } catch (const std::exception& e) {
        if (fd != -1)
            close(fd);
        TRACE(Trace::error, e.what());
        THROW(Error::GENERAL_ERROR);
    }

    return statbuf.st_size;
}

bool SelRecall::processFiles(std::string tapeId, FsObj::file_state toState,
bool needsTape)

{
    SQLStatement stmt;
    std::string fileName;
    FsObj::file_state state;
    unsigned long inum;
    std::shared_ptr<LTFSDMDrive> drive = nullptr;
    std::list<unsigned long> inumList;
    bool suspended = false;
    time_t start;

    TRACE(Trace::full, reqNumber);

    {
        std::lock_guard<std::mutex> lock(Scheduler::updmtx);
        Scheduler::updReq[reqNumber] = true;
        Scheduler::updcond.notify_all();
    }

    if (needsTape) {
        for (std::shared_ptr<LTFSDMDrive> d : inventory->getDrives()) {
            if (d->get_slot() == inventory->getCartridge(tapeId)->get_slot()) {
                drive = d;
                break;
            }
        }
        assert(drive != nullptr);
    }

    stmt(SelRecall::SET_RECALLING) << FsObj::RECALLING_MIG << reqNumber
            << FsObj::MIGRATED << tapeId;
    TRACE(Trace::normal, stmt.str());
    stmt.doall();

    stmt(SelRecall::SET_RECALLING) << FsObj::RECALLING_PREMIG << reqNumber
            << FsObj::PREMIGRATED << tapeId;
    TRACE(Trace::normal, stmt.str());
    stmt.doall();

    stmt(SelRecall::SELECT_JOBS) << reqNumber << tapeId << FsObj::RECALLING_MIG
            << FsObj::RECALLING_PREMIG;
    TRACE(Trace::normal, stmt.str());
    stmt.prepare();
    start = time(NULL);
    while (stmt.step(&fileName, &state, &inum)) {
        if (Server::terminate == true)
            break;

        if (state == FsObj::RECALLING_MIG)
            state = FsObj::MIGRATED;
        else
            state = FsObj::PREMIGRATED;

        TRACE(Trace::always, fileName, state, toState);

        if (state == toState)
            continue;

        if (needsTape && drive->getToUnblock() == DataBase::TRARECALL) {
            TRACE(Trace::always, tapeId);
            suspended = true;
            break;
        }

        try {
            if ((state == FsObj::MIGRATED) && (needsTape == false)) {
                MSG(LTFSDMS0047E, fileName);
                THROW(Error::GENERAL_ERROR, fileName);
            }
            recall(fileName, tapeId, state, toState);
            inumList.push_back(inum);
            mrStatus.updateSuccess(reqNumber, state, toState);
        } catch (const std::exception& e) {
            TRACE(Trace::error, e.what());
            mrStatus.updateFailed(reqNumber, state);
            SQLStatement failstmt = SQLStatement(SelRecall::FAIL_JOB)
                    << FsObj::FAILED << fileName << reqNumber << tapeId;

            TRACE(Trace::error, stmt.str());
            failstmt.doall();
        }

        if (time(NULL) - start < 10)
            continue;

        start = time(NULL);

        std::lock_guard<std::mutex> lock(Scheduler::updmtx);
        Scheduler::updReq[reqNumber] = true;
        Scheduler::updcond.notify_all();
    }
    {
        std::lock_guard<std::mutex> lock(Scheduler::updmtx);
        Scheduler::updReq[reqNumber] = true;
        Scheduler::updcond.notify_all();
    }
    stmt.finalize();

    stmt(SelRecall::SET_JOB_SUCCESS) << toState << reqNumber << tapeId
            << FsObj::RECALLING_MIG << FsObj::RECALLING_PREMIG
            << genInumString(inumList);
    TRACE(Trace::normal, stmt.str());
    stmt.doall();

    stmt(SelRecall::RESET_JOB_STATE) << FsObj::MIGRATED << reqNumber << tapeId
            << FsObj::RECALLING_MIG;
    TRACE(Trace::normal, stmt.str());
    stmt.doall();

    stmt(SelRecall::RESET_JOB_STATE) << FsObj::PREMIGRATED << reqNumber
            << tapeId << FsObj::RECALLING_PREMIG;
    TRACE(Trace::normal, stmt.str());
    stmt.doall();

    return suspended;
}

void SelRecall::execRequest(std::string tapeId, bool needsTape)

{
    SQLStatement stmt;
    bool suspended = false;

    mrStatus.add(reqNumber);

    if (targetState == LTFSDmProtocol::LTFSDmSelRecRequest::PREMIGRATED)
        suspended = processFiles(tapeId, FsObj::PREMIGRATED, needsTape);
    else
        suspended = processFiles(tapeId, FsObj::RESIDENT, needsTape);

    std::unique_lock<std::mutex> lock(Scheduler::mtx);

    TRACE(Trace::always, reqNumber, needsTape, tapeId);

    if (needsTape) {
        std::lock_guard<std::recursive_mutex> lock(LTFSDMInventory::mtx);
        inventory->getCartridge(tapeId)->setState(
                LTFSDMCartridge::TAPE_MOUNTED);
        bool found = false;
        for (std::shared_ptr<LTFSDMDrive> d : inventory->getDrives()) {
            if (d->get_slot() == inventory->getCartridge(tapeId)->get_slot()) {
                TRACE(Trace::always, d->GetObjectID());
                d->setFree();
                d->clearToUnblock();
                found = true;
                break;
            }
        }
        assert(found == true);
    }

    std::unique_lock<std::mutex> updlock(Scheduler::updmtx);

    stmt(SelRecall::UPDATE_REQUEST)
            << (suspended ? DataBase::REQ_NEW : DataBase::REQ_COMPLETED)
            << reqNumber << tapeId;
    TRACE(Trace::normal, stmt.str());
    stmt.doall();

    Scheduler::updReq[reqNumber] = true;
    Scheduler::updcond.notify_all();
    Scheduler::cond.notify_one();
}
