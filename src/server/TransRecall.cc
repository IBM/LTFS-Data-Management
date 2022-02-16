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

/** @page transparent_recall Transparent Recall

    # TransRecall

    For each file system that is managed with LTFS Data Management a
    Fuse overlay file system is created. After that the original file
    system should not be used anymore by an user or by an application:
    only the Fuse overlay file system should be used instead. For each
    of these Fuse overlay file systems an additional process is started:

    @dot
    digraph ltfsdm_processes {
        compound=true;
        fontname="courier";
        fontsize=11;
        labeljust=l;
        node [shape=record, width=2, fontname="courier", fontsize=11, fillcolor=white, style=filled];
        ltfsdmd [label="ltfsdmd (backend)"];
        ltfsdmd_fs1 [label="ltfsdmd.ofs -m /mnt/fs1"];
        ltfsdmd_fs2 [label="ltfsdmd.ofs -m /mnt/fs2"];
        ltfsdmd_fs3 [label="ltfsdmd.ofs -m /mnt/fs3"];
        ltfsdmd -> ltfsdmd_fs1 [];
        ltfsdmd -> ltfsdmd_fs2 [];
        ltfsdmd -> ltfsdmd_fs3 [];
    }
    @enddot

    Within the Fuse processes read, write, and truncate calls on premigrated
    or migrated files are intercepted since there is a requirement to recall
    (transfer data back from tape to disk) data or to perform a file state
    change. The Fuse overlay file system as part of the Fuse connector is
    described in more detail at @ref fuse_connector.

    The data transfer and the file system change of a file are performed
    within the backend. Therefore there needs to be a communication between
    the Fuse overlay file system processes and the backend regarding files
    to recall. The communication method is the same like between the client
    and the backend: local socket communication and Google protocol buffers
    for message serialization:

    @dot
    digraph trec_communication {
        compound=true;
        fontname="courier";
        fontsize=11;
        labeljust=l;
        rankdir=LR;
        node [shape=record, width=2, fontname="courier", fontsize=11, fillcolor=white, style=filled];
        ltfsdmd_fs1 [label="ltfsdmd.ofs -m /mnt/fs1"];
        ltfsdmd [label="ltfsdmd (backend)"];
        ltfsdmd_fs1 -> ltfsdmd [fontname="courier", fontsize=8, fontcolor=dodgerblue4,
            label="1) Connector::getEvents()\nLTFSDmProtocol::LTFSDmTransRecRequest",
            URL="@ref Connector::getEvents"];
        ltfsdmd -> ltfsdmd_fs1 [fontname="courier", fontsize=8, fontcolor=dodgerblue4,
            label="2) Connector::respondRecallEvent()\nLTFSDmProtocol::LTFSDmTransRecResp",
            URL="@ref Connector::respondRecallEvent" ];
    }
    @enddot

    If a recall request LTFSDmProtocol::LTFSDmTransRecRequest has been sent
    to the backend within the Fuse process read, write, and truncate
    processing is blocked until the backend responds with
    Connector::respondRecallEvent.


    The transparent recall processing within the backend happens within two phases:

    1. One backend thread ("RecallD" executing TransRecall::run) waits on a
       socket for recall events. Recall events are are initiated by
       applications that perform read, write, or truncate calls on a
       premigrated or migrated files. A corresponding
       job is created within the JOB_QUEUE table and - if it does not exist - a
       request is created within the REQUEST_QUEUE table.
    2. The Scheduler identifies a transparent recall request to get scheduled.
       The order of files being recalled depends on the starting block of
       the data files on tape: @snippet server/SQLStatements.cc trans_recall_sql_qry
       If the transparent recall job is finally processed (even it is failed)
       the event is responded  as a Protocol Buffers message
       (LTFSDmProtocol::LTFSDmTransRecResp).

    @dot
    digraph trans_recall {
        compound=true;
        fontname="courier";
        fontsize=11;
        labeljust=l;
        node [shape=record, width=2, fontname="courier", fontsize=11, fillcolor=white, style=filled];
        subgraph cluster_first {
            label="first phase";
            recv [label="receive event"];
            ajr [label="add job, add\nrequest if not exists"];
            recv -> ajr [];
        }
        subgraph cluster_second {
            label="second phase";
            scheduler [label="Scheduler"];
            subgraph cluster_rec_exec {
                label="SelRecall::execRequest";
                subgraph cluster_proc_files {
                    label="SelRecall::processFiles";
                    rec_exec [label="{read from tape\n\nordered by starting block|respond event}"];
                }
            }
            scheduler -> rec_exec [label="schedule\nrecall request", fontname="courier", fontsize=8, lhead=cluster_rec_exec];
        }
        subgraph cluster_tables {
            label="SQLite tables";
            tables [label="<rq> REQUEST_QUEUE|<jq> JOB_QUEUE"];
        }
        ajr ->  tables [color=darkgreen, fontcolor=darkgreen, label="add", fontname="courier", fontsize=8, headport=w, lhead=cluster_tables];
        scheduler -> tables:rq [color=darkgreen, fontcolor=darkgreen, label="check for requests to schedule", fontname="courier", fontsize=8, tailport=e];
        rec_exec -> tables [color=darkgreen, fontcolor=darkgreen, label="read and update", fontname="courier", fontsize=8, lhead=cluster_tables, ltail=cluster_rec_exec];
        ajr -> scheduler [color=blue, fontcolor=blue, label="condition", fontname="courier", fontsize=8];
        scheduler -> ajr [style=invis]; // just for the correct order of the subgraphs
    }
    @enddot

    This high level description is explained in more detail in the following subsections.

    If there are multiple recall events for files on the same tape
    only one request is created within the REQUEST_QUEUE table. This
    request is removed if there are no further outstanding transparent
    recalls for the same tape. If there is a new transparent recall
    event and if a corresponding request already exists within the
    REQUEST_QUEUE table this existing request is used for further
    processing this request/event.

    The second step will not start before the first step is completed. For
    the second step the required tape and drive resources need to be
    available: e.g. a corresponding cartridge is mounted on a tape drive.
    The second phase may start immediately after the first phase but it also
    can take a longer time depending when a required resource gets available.

    ## 1. adding jobs and requests to the internal tables

    One backend thread exists (see @ref server_code) that executes the
    TransRecall::run method to wait for recall events. Recall events are sent
    as Protocol Buffers messages (LTFSDmProtocol::LTFSDmTransRecRequest) over a
    socket. The information provided contains the following:

    - opaque information specific to the connector
    - an indicator if a file should be recall to premigrated or to
      resident state
    - the file uid (see fuid_t)
    - the file name

    Thereafter the tape id for the first tape listed within the attributes
    is obtained. The recall will happen from that tape. There currently is
    no optimization if the file has been migrated to more than one tape to
    select between these tapes in an optimal way.

    To add a corresponding job within the JOB_QUEUE table or if necessary
    a request within the REQUEST_QUEUE table an additional thread is used
    as part of the ThreadPool wqr executing the method TransRecall::addJob.

    This is an example of these two tables in case of transparently recalling a few files:

    @verbatim
    sqlite> select * from JOB_QUEUE;
    OPERATION   FILE_NAME               REQ_NUM     TARGET_STATE  REPL_NUM    TAPE_POOL   FILE_SIZE   FS_ID_H               FS_ID_L               I_GEN       I_NUM       MTIME_SEC   MTIME_NSEC  LAST_UPD    TAPE_ID     FILE_STATE  START_BLOCK  CONN_INFO
    ----------  ----------------------  ----------  ------------  ----------  ----------  ----------  --------------------  --------------------  ----------  ----------  ----------  ----------  ----------  ----------  ----------  -----------  ---------------
    0           /mnt/lxfs/test2/file.4  3           1             -1                      32768       -4229860921927319251  -6765444084672883911  1380830956  788454      1514304375  0           1515591742  D01301L5    6           199882       139980701699680
    0           /mnt/lxfs/test2/file.2  3           1             -1                      32768       -4229860921927319251  -6765444084672883911  -316887448  788450      1514304375  0           1515591742  D01301L5    6           206251       139980701699984
    0           /mnt/lxfs/test2/file.0  3           1             -1                      32768       -4229860921927319251  -6765444084672883911  -188757665  787590      1514304375  0           1515591742  D01301L5    6           207111       139980701700096
    sqlite> select * from REQUEST_QUEUE;
    OPERATION   REQ_NUM     TARGET_STATE  NUM_REPL    REPL_NUM    TAPE_POOL   TAPE_ID     TIME_ADDED  STATE
    ----------  ----------  ------------  ----------  ----------  ----------  ----------  ----------  ----------
    0           3                                                             D01301L5    1515591742  1
    @endverbatim

    For a description of the columns see @ref sqlite.

    The following is an overview of this initial transparent recall processing
    including corresponding links to the code parts:

    <TT>
    TransRecall::run:
    - while not terminating (Connector::connectorTerminate == false)
        - wait for events: Connector::getEvents
        - create FsObj object according the recall information recinfo
        - determine the id of the first cartridge from the attributes
        - enqueue the job and request creation   as part of the
          ThreadPool wqr executing the method TransRecall::addJob.

    </TT>
    <TT>
    TransRecall::addJob:
    - determine path name on tape
    - add a job within the JOB_QUEUE table
    - if a request already exists: if (reqExists == true)
        - change request state to new
    - else
        - create a request within the REQUEST_QUEUE table

    </TT>

    ## 2. Scheduling transparent recall jobs

    After a transparent recall request is ready to be scheduled and
    there is a free tape and drive resource available to schedule this
    transparent recall request the following will happen:

    <TT>
    Scheduler::run:
    - if a transparent request is ready do be scheduled:
        - update record in request queue to mark it as DataBase::REQ_INPROGRESS
        - TransRecall::execRequest
            - call TransRecall::processFiles
                - respond recall event Connector::respondRecallEvent
            - if there are outstanding transparent recall requests for the same tape (remaining)
                - update record in request queue to mark it as DataBase::REQ_NEW
            - else
                - delete request within the REQUEST_QUEUE table

    </TT>

    ### TransRecall::processFiles

    The TransRecall::processFiles method is traversing the JOB_QUEUE table to
    process individual files for transparent recall. In general the following
    steps are performed:

    -# All corresponding jobs are changed to FsObj::RECALLING_MIG or FsObj::RECALLING_PREMIG
       depending if it is called for files in migrated or in premigrated state. The following
       example shows this change regarding six files:
       @dot
       digraph step_1 {
            compound=true;
            fontname="courier";
            fontsize=11;
            rankdir=LR;
            node [shape=record, width=2, fontname="courier", fontsize=11, fillcolor=white, style=filled];
            before [label="file.1: FsObj::MIGRATED|file.2: FsObj::PREMIGRATED|file.3: FsObj::MIGRATED|file.4: FsObj::PREMIGRATED|file.5: FsObj::MIGRATED|file.6: FsObj::PREMIGRATED"];
            after [label="file.1: FsObj::RECALLING_MIG|file.2: FsObj::RECALLING_PREMIG|file.3: FsObj::RECALLING_MIG|file.4: FsObj::RECALLING_PREMIG|file.5: FsObj::RECALLING_MIG|file.6: FsObj::RECALLING_PREMIG"];
            before -> after [];
       }
       @enddot
    -# Process all these jobs in FsObj::RECALLING_MIG or FsObj::RECALLING_PREMIG state
       which results in the recall of all corresponding files. For all jobs in
       FsObj::RECALLING_PREMIG state there will no data transfer happen.
       The result of each individual transparent recall is stored in a
       respinfo_t respinfo std::list object (no change within the JOB_QUEUE table):
       @dot
       digraph step_1 {
            compound=true;
            fontname="courier";
            fontsize=11;
            rankdir=LR;
            node [shape=record, width=2, fontname="courier", fontsize=11, fillcolor=white, style=filled];
            before [label="file.1: FsObj::RECALLING_MIG|file.2: FsObj::RECALLING_PREMIG|file.3: FsObj::RECALLING_MIG|file.4: FsObj::RECALLING_PREMIG|file.5: FsObj::RECALLING_MIG|file.6: FsObj::RECALLING_PREMIG"];
            after [label="file.1: FsObj::RECALLING_MIG|file.2: FsObj::RECALLING_PREMIG|file.3: FsObj::RECALLING_MIG|file.4: FsObj::RECALLING_PREMIG|file.5: FsObj::RECALLING_MIG|file.6: FsObj::RECALLING_PREMIG"];
            before -> after [];
       }
       @enddot
    -# The corresponding jobs are deleted from the JOB_QUEUE table:
       @dot
       digraph step_1 {
            compound=true;
            fontname="courier";
            fontsize=11;
            rankdir=LR;
            node [shape=record, width=2, fontname="courier", fontsize=11, fillcolor=white, style=filled];
            before [label="file.1: FsObj::RECALLING_MIG|file.2: FsObj::RECALLING_PREMIG|file.3: FsObj::RECALLING_MIG|file.4: FsObj::FAILED|file.5: FsObj::RECALLING_MIG|file.6: FsObj::RECALLING_PREMIG"];
            after [fontcolor=lightgrey, label="file.1: FsObj::RECALLING_MIG|file.2: FsObj::RECALLING_PREMIG|file.3: FsObj::RECALLING_MIG|file.4: FsObj::FAILED|file.5: FsObj::RECALLING_MIG|file.6: FsObj::RECALLING_PREMIG"];
            before -> after [];
       }
       @enddot
    -# All entries within the respinfo_t respinfo std::list object are
       responded if processing was successful or not (respinfo.succeeded)
       by calling Connector::respondRecallEvent.

    In opposite to migration recalls are not performed in parallel.
    For an optimal performance the data should be read serially from
    tape in the order of the starting block of each data file.

    ### TransRecall::recall

    Recalling an individual file is performed according the following steps:

    -# If state is FsObj::MIGRATED data is read in a loop from tape and written to disk.
    -# The attributes on the disk file are updated or removed in the case of target state resident.
 */

void TransRecall::addJob(Connector::rec_info_t recinfo, std::string tapeId,
        long reqNum)

{
    struct stat statbuf;
    SQLStatement stmt;
    std::string tapeName;
    int state;
    FsObj::mig_target_attr_t attr;
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
            MSG(LTFSDMS0032E, recinfo.fuid.inum);
            return;
        }

        state = fso.getMigState();

        if (state == FsObj::RESIDENT) {
            MSG(LTFSDMS0031I, recinfo.fuid.inum);
            Connector::respondRecallEvent(recinfo, true);
            return;
        }

        attr = fso.getAttribute();

        tapeName = Server::getTapeName(recinfo.fuid.fsid_h, recinfo.fuid.fsid_l,
                recinfo.fuid.igen, recinfo.fuid.inum, tapeId);
    } catch (const std::exception& e) {
        TRACE(Trace::error, e.what());
        if (filename.compare("NULL") != 0)
            MSG(LTFSDMS0073E, filename);
        else
            MSG(LTFSDMS0032E, recinfo.fuid.inum);
    }

    stmt(TransRecall::ADD_JOB) << DataBase::TRARECALL << filename.c_str()
            << reqNum
            << (recinfo.toresident ? FsObj::RESIDENT : FsObj::PREMIGRATED)
            << Const::UNSET << statbuf.st_size << recinfo.fuid.fsid_h
            << recinfo.fuid.fsid_l << recinfo.fuid.igen << recinfo.fuid.inum
            << statbuf.st_mtime << 0 << time(NULL) << state << tapeId
            << attr.tapeInfo[0].startBlock << (std::intptr_t) recinfo.conn_info;

    TRACE(Trace::normal, stmt.str());

    stmt.doall();

    if (filename.compare("NULL") != 0)
        TRACE(Trace::always, filename);
    else
        TRACE(Trace::always, recinfo.fuid.inum);

    TRACE(Trace::always, tapeId);

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
        Scheduler::invoke();
    } else {
        stmt(TransRecall::ADD_REQUEST) << DataBase::TRARECALL << reqNum
                << Const::UNSET << attr.tapeInfo[0].tapeId << time(NULL)
                << DataBase::REQ_NEW;
        TRACE(Trace::normal, stmt.str());
        stmt.doall();
        Scheduler::invoke();
    }
}

void TransRecall::cleanupEvents()

{
    Connector::rec_info_t recinfo;
    SQLStatement stmt = SQLStatement(TransRecall::REMAINING_JOBS)
            << DataBase::TRARECALL;
    TRACE(Trace::normal, stmt.str());
    stmt.prepare();
    while (stmt.step(&recinfo.fuid.fsid_h, &recinfo.fuid.fsid_l,
            &recinfo.fuid.igen, &recinfo.fuid.inum, &recinfo.filename,
            (std::intptr_t *) &recinfo.conn_info)) {
        TRACE(Trace::always, recinfo.filename, recinfo.fuid.inum);
        Connector::respondRecallEvent(recinfo, false);
    }
    stmt.finalize();
}

void TransRecall::run(std::shared_ptr<Connector> connector)

{
    ThreadPool<TransRecall, Connector::rec_info_t, std::string, long> wqr(
            &TransRecall::addJob, Const::MAX_TRANSPARENT_RECALL_THREADS,
            "trec-wq");
    Connector::rec_info_t recinfo;
    std::map<std::string, long> reqmap;
    std::string tapeId;

    try {
        connector->initTransRecalls();
    } catch (const std::exception& e) {
        TRACE(Trace::error, e.what());
        MSG(LTFSDMS0030E);
        return;
    }

    try {
        FileSystems fss;
        for (std::string fs : Server::conf.getFss()) {
            try {
                FsObj fileSystem(fs);
				FileSystems::fsinfo fsi = Server::conf.getFs(fs);
                if (fileSystem.isFsManaged()) {
                    MSG(LTFSDMS0042I, fs);
                    fileSystem.manageFs(true, connector->getStartTime(), fsi.automig, fsi.pool);
                }
            } catch (const LTFSDMException& e) {
                TRACE(Trace::error, e.what());
                switch (e.getError()) {
                    case Error::FS_CHECK_ERROR:
                        MSG(LTFSDMS0044E, fs);
                        break;
                    case Error::FS_ADD_ERROR:
                        MSG(LTFSDMS0045E, fs);
                        break;
                    default:
                        MSG(LTFSDMS0045E, fs);
                }
            } catch (const std::exception& e) {
                TRACE(Trace::error, e.what());
            }
        }
    } catch (const std::exception& e) {
        MSG(LTFSDMS0079E, e.what());
    }

    while (Connector::connectorTerminate == false) {
        try {
            recinfo = connector->getEvents();
        } catch (const std::exception& e) {
            MSG(LTFSDMS0036W, e.what());
        }

        // is sent for termination
        if (recinfo.conn_info == NULL) {
            TRACE(Trace::always, recinfo.fuid.inum);
            continue;
        }

        if (Server::terminate == true) {
            TRACE(Trace::always, (bool) Server::terminate);
            connector->respondRecallEvent(recinfo, false);
            continue;
        }

        if (recinfo.fuid.inum == 0) {
            TRACE(Trace::always, recinfo.fuid.inum);
            continue;
        }

        // error case: managed region set but no attrs
        try {
            FsObj fso(recinfo);

            if (fso.getMigState() == FsObj::RESIDENT) {
                fso.finishRecall(FsObj::RESIDENT);
                MSG(LTFSDMS0039I, recinfo.fuid.inum);
                connector->respondRecallEvent(recinfo, true);
                continue;
            }

            tapeId = fso.getAttribute().tapeInfo[0].tapeId;
        } catch (const LTFSDMException& e) {
            TRACE(Trace::error, e.what());
            if (e.getError() == Error::ATTR_FORMAT)
                MSG(LTFSDMS0037W, recinfo.fuid.inum);
            else
                MSG(LTFSDMS0038W, recinfo.fuid.inum, e.getErrno());
            connector->respondRecallEvent(recinfo, false);
            continue;
        } catch (const std::exception& e) {
            TRACE(Trace::error, e.what());
            connector->respondRecallEvent(recinfo, false);
            continue;
        }

        std::stringstream thrdinfo;
        thrdinfo << "TrRec(" << recinfo.fuid.inum << ")";

        if (reqmap.count(tapeId) == 0)
            reqmap[tapeId] = ++globalReqNumber;

        TRACE(Trace::always, recinfo.fuid.inum, tapeId, reqmap[tapeId]);

        wqr.enqueue(Const::UNSET, TransRecall(), recinfo, tapeId,
                reqmap[tapeId]);
    }

    MSG(LTFSDMS0083I);
    connector->endTransRecalls();
    wqr.waitCompletion(Const::UNSET);
    cleanupEvents();
    MSG(LTFSDMS0084I);
}

unsigned long TransRecall::recall(Connector::rec_info_t recinfo,
        std::string tapeId, FsObj::file_state state, FsObj::file_state toState)

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
        FsObj target(recinfo);

        TRACE(Trace::always, recinfo.fuid.inum, recinfo.filename);

        std::lock_guard<FsObj> fsolock(target);

        curstate = target.getMigState();

        if (curstate != state) {
            MSG(LTFSDMS0034I, recinfo.fuid.inum);
            state = curstate;
        }
        if (state == FsObj::RESIDENT) {
            return 0;
        } else if (state == FsObj::MIGRATED) {
            tapeName = Server::getTapeName(recinfo.fuid.fsid_h,
                    recinfo.fuid.fsid_l, recinfo.fuid.igen, recinfo.fuid.inum,
                    tapeId);
            fd = Server::openTapeRetry(tapeId, tapeName.c_str(),
            O_RDWR | O_CLOEXEC);

            if (fd == -1) {
                TRACE(Trace::error, errno);
                MSG(LTFSDMS0021E, tapeName.c_str());
                THROW(Error::GENERAL_ERROR, tapeName, errno);
            }

            statbuf = target.stat();

            if (fstat(fd, &statbuf_tape) == 0
                    && statbuf_tape.st_size != statbuf.st_size) {
                if (recinfo.filename.size() != 0)
                    MSG(LTFSDMS0097W, recinfo.filename, statbuf.st_size,
                            statbuf_tape.st_size);
                else
                    MSG(LTFSDMS0098W, recinfo.fuid.inum, statbuf.st_size,
                            statbuf_tape.st_size);
                statbuf.st_size = statbuf_tape.st_size;
                toState = FsObj::RESIDENT;
            }

            target.prepareRecall();

            while (offset < statbuf.st_size) {
                if (Server::forcedTerminate)
                    THROW(Error::GENERAL_ERROR, tapeName);

                rsize = read(fd, buffer, sizeof(buffer));
                if (rsize == 0) {
                    break;
                }
                if (rsize == -1) {
                    TRACE(Trace::error, errno);
                    MSG(LTFSDMS0023E, tapeName.c_str());
                    THROW(Error::GENERAL_ERROR, tapeName, errno);
                }
                wsize = target.write(offset, (unsigned long) rsize, buffer);
                if (wsize != rsize) {
                    TRACE(Trace::error, errno, wsize, rsize);
                    MSG(LTFSDMS0033E, recinfo.fuid.inum);
                    close(fd);
                    THROW(Error::GENERAL_ERROR, recinfo.fuid.inum, wsize,
                            rsize);
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
        THROW(Error::GENERAL_ERROR);
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
    while (stmt.step(&recinfo.fuid.fsid_h, &recinfo.fuid.fsid_l,
            &recinfo.fuid.igen, &recinfo.fuid.inum, &recinfo.filename, &state,
            &toState, (std::intptr_t *) &recinfo.conn_info)) {
        numFiles++;

        if (state == FsObj::RECALLING_MIG)
            state = FsObj::MIGRATED;
        else
            state = FsObj::PREMIGRATED;

        if (toState == FsObj::RESIDENT)
            recinfo.toresident = true;

        TRACE(Trace::always, recinfo.filename, recinfo.fuid.inum, state,
                toState);

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

void TransRecall::execRequest(int reqNum, std::string driveId,
        std::string tapeId)

{
    SQLStatement stmt;
    int remaining = 0;

    TRACE(Trace::always, reqNum, tapeId);

    processFiles(reqNum, tapeId);

    {
        std::lock_guard<std::recursive_mutex> inventorylock(
                LTFSDMInventory::mtx);
        if (inventory->getCartridge(tapeId)->getState()
                == LTFSDMCartridge::TAPE_INUSE)
            inventory->getCartridge(tapeId)->setState(
                    LTFSDMCartridge::TAPE_MOUNTED);

        inventory->getDrive(driveId)->setFree();
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
    Scheduler::invoke();
}
