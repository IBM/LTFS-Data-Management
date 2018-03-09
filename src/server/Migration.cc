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

/** @page migration Migration

    # Migration

    The migration processing happens within two phases:

    1. The backend receives a migration message which is further processed
       within the MessageParser::migrationMessage method. During this
       processing migration jobs and migration requests are added to
       the internal queues.
    2. The Scheduler identifies a migration request to scheduled this request.\n
       The migration happens within the following sequence:
       - The corresponding data is written to the selected tape.
       - The tape index is synchronized.
       - The corresponding files on disk are stubbed (for migrated
         state as target only) and the migration state is changed.


    @dot
    digraph migration {
        compound=true;
        fontname="courier";
        fontsize=11;
        labeljust=l;
        node [shape=record, width=2, fontname="courier", fontsize=11, fillcolor=white, style=filled];
        subgraph cluster_first {
            label="first phase";
            recv [label="Receiver"];
            msgparser [label="MessageParser"];
            recv -> msgparser [];
        }
        subgraph cluster_second {
            label="second phase";
            scheduler [label="Scheduler"];
            subgraph cluster_mig_exec {
                label="Migration::execRequest";
                mig_exec [label="{ <write_to_tape> write to tape\n(Migration::transferData)|<sync_index> sync index|<stub> stub files\n(Migration::changeFileState)}"];
            }
            scheduler -> mig_exec [label="schedule\nmigration request", fontname="courier", fontsize=8, lhead=cluster_mig_exec];
        }
        subgraph cluster_tables {
            label="SQLite tables";
            tables [label="<rq> REQUEST_QUEUE|<jq> JOB_QUEUE"];
        }
        msgparser -> scheduler [color=blue, fontcolor=blue, label="condition", fontname="courier", fontsize=8];
        scheduler -> msgparser [style=invis]; // just for the correct order of the subgraphs
        msgparser -> tables [color=darkgreen, fontcolor=darkgreen, label="add", fontname="courier", fontsize=8, headport=w, lhead=cluster_tables];
        scheduler -> tables:rq [color=darkgreen, fontcolor=darkgreen, label="check for requests to schedule", fontname="courier", fontsize=8, tailport=e];
        mig_exec -> tables [color=darkgreen, fontcolor=darkgreen, label="read and update", fontname="courier", fontsize=8, lhead=cluster_tables, ltail=cluster_mig_exec];
    }
    @enddot

    This high level description is explained in more detail in the following
    subsections.

    The second step will not start before the first step is completed. For
    the second step the required tape and drive resources need to be
    available: e.g. a corresponding cartridge is mounted on a tape drive.
    The second phase may start immediately after the first phase but it also
    can take a longer time depending when a required resource gets available.

    ## 1. adding jobs and requests to the internal tables

    When a client sends a migration request to the backend the corresponding
    information is split into two parts. The first part contains information
    that is relevant for the whole request:

    - the tape storage pools the migration is targeted to
    - the target migration state (premigrated or migrated)

    Thereafter the file names of the files to be migrated are sent to the backend.
    When receiving this information corresponding entries are added to the SQL
    table JOB_QUEUE. For each file name one or more jobs are created based
    on the number of tape storage pools being specified. After that: entries are added
    to the SQL table REQUEST_QUEUE. For each storage pool being specified a
    corresponding entry is added to that table.

    The following is an example of the two tables when migrating four files to
    two pools:

    @verbatim
    sqlite> select * from JOB_QUEUE;
    OPERATION   FILE_NAME               REQ_NUM     TARGET_STATE  REPL_NUM    TAPE_POOL   FILE_SIZE   FS_ID_H               FS_ID_L               I_GEN        I_NUM       MTIME_SEC   MTIME_NSEC  LAST_UPD    TAPE_ID     FILE_STATE  START_BLOCK  CONN_INFO
    ----------  ----------------------  ----------  ------------  ----------  ----------  ----------  --------------------  --------------------  -----------  ----------  ----------  ----------  ----------  ----------  ----------  -----------  ----------
    2           /mnt/lxfs/test3/file.1  6           1             0           pool_1      32768       -4229860921927319251  -6765444084672883911  -1926452355  1084047255  1514983931  500913649   1514983959              0
    2           /mnt/lxfs/test3/file.1  6           1             1           pool_2      32768       -4229860921927319251  -6765444084672883911  -1926452355  1084047255  1514983931  500913649   1514983959              0
    2           /mnt/lxfs/test3/file.2  6           1             0           pool_1      32768       -4229860921927319251  -6765444084672883911  1442398178   1083519761  1514983931  504913625   1514983959              0
    2           /mnt/lxfs/test3/file.2  6           1             1           pool_2      32768       -4229860921927319251  -6765444084672883911  1442398178   1083519761  1514983931  504913625   1514983959              0
    2           /mnt/lxfs/test3/file.3  6           1             0           pool_1      32768       -4229860921927319251  -6765444084672883911  -1348613989  1074666489  1514983931  507913608   1514983959              0
    2           /mnt/lxfs/test3/file.3  6           1             1           pool_2      32768       -4229860921927319251  -6765444084672883911  -1348613989  1074666489  1514983931  507913608   1514983959              0
    2           /mnt/lxfs/test3/file.4  6           1             0           pool_1      32768       -4229860921927319251  -6765444084672883911  -1199258554  1074666490  1514983931  510913590   1514983959              0
    2           /mnt/lxfs/test3/file.4  6           1             1           pool_2      32768       -4229860921927319251  -6765444084672883911  -1199258554  1074666490  1514983931  510913590   1514983959              0
    sqlite> select * from REQUEST_QUEUE;
    OPERATION   REQ_NUM     TARGET_STATE  NUM_REPL    REPL_NUM    TAPE_POOL   TAPE_ID     TIME_ADDED  STATE
    ----------  ----------  ------------  ----------  ----------  ----------  ----------  ----------  ----------
    2           6           1             2           0           pool_1                  1514983959  0
    2           6           1             2           1           pool_2                  1514983959  0
    @endverbatim

    For a description of the columns see @ref sqlite.

    During the scheduling later-on the cartridges will be identified as
    parts of the tape storage pools.

    The following is an overview of this initial migration processing including
    corresponding links to the code parts:

    <TT>
    MessageParser::migrationMessage:
    - retrieve tape storage pool information (@ref LTFSDmProtocol::LTFSDmMigRequest::pools "migreq.pools()")
    - retrieve target state information (premigrated or migrated state, @ref LTFSDmProtocol::LTFSDmMigRequest::state "migreq.state()")
    - create a Migration object
    - respond back to the client with a request number
    - MessageParser::getObjects: retrieving file names to migrate
        - Migration::addJob: add migration information the the SQLite table JOB_QUEUE
    - Migration::addRequest: add a request to the SQLite table REQUEST_QUEUE
    - MessageParser::reqStatusMessage: provide updates of the migration processing to the client

    </TT>

    ## 2. Scheduling migration jobs

    After a migration request has been added to the REQUEST_QUEUE and
    there is a free tape and drive resource available to schedule this
    migration request the following will happen:

    <TT>
    In Scheduler::run if a migration request is ready do be scheduled:
      - update record in request queue to mark it as DataBase::REQ_INPROGRESS
      - Migration::execRequest
          - if @ref Migration::needsTape "needsTape" is true:
              - Migration::processFiles to transfer data to tape
                  - Migration::transferData: transfer the data to tape of
                    all files according this request
                  - synchronize tape index
                  - release tape for further operations since for stubbing
                    files there is nothing written to tape
          - Migration::processFiles to change the file state
              - Migration::changeFileState: stub all corresponding files
                (for migrated state as target only) and perform the change
                of the migration state
          - update record in request queue to mark it as DataBase::REQ_COMPLETED

    </TT>

    In Migration::execRequest adding an entry to the @ref mrStatus object is
    necessary for the client that initiated the request to receive progress
    information.

    ### Migration::processFiles

    The Migration::processFiles method is called twice first to transfer the
    file data and a second time to stub them if necessary and to perform
    the change of the migration state . If all files
    to be processed are already premigrated there is no need to mount a
    cartridge. In this case the data transfer step is skipped. If the
    target state is LTFSDmProtocol::LTFSDmMigRequest::PREMIGRATED files
    are not stubbed and only the migration state is changed to premigrated.

    The Migration::processFiles method in general perform the following
    steps:

    -# All corresponding jobs are changed to FsObj::TRANSFERRING or
       FsObj::CHANGINGFSTATE depending on the migration phase. The following
       example shows this change for the first phase of six files:
       @dot
       digraph step_1 {
            compound=true;
            fontname="courier";
            fontsize=11;
            rankdir=LR;
            node [shape=record, width=2, fontname="courier", fontsize=11, fillcolor=white, style=filled];
            before [label="file.1: FsObj::RESIDENT|file.2: FsObj::RESIDENT|file.3: FsObj::RESIDENT|file.4: FsObj::RESIDENT|file.5: FsObj::RESIDENT|file.6: FsObj::RESIDENT"];
            after [label="file.1: FsObj::TRANSFERRING|file.2: FsObj::TRANSFERRING|file.3: FsObj::TRANSFERRING|file.4: FsObj::TRANSFERRING|file.5: FsObj::TRANSFERRING|file.6: FsObj::TRANSFERRING"];
            before -> after [];
       }
       @enddot
    -# Process all these jobs in FsObj::TRANSFERRING or FsObj::CHANGINGFSTATE state
       which results in the data transfer or stubbing of all corresponding files.
       The following change indicates that the data transfer of file file.4 failed:
       @dot
       digraph step_1 {
            compound=true;
            fontname="courier";
            fontsize=11;
            rankdir=LR;
            node [shape=record, width=2, fontname="courier", fontsize=11, fillcolor=white, style=filled];
            before [label="file.1: FsObj::TRANSFERRING|file.2: FsObj::TRANSFERRING|file.3: FsObj::TRANSFERRING|file.4: FsObj::TRANSFERRING|file.5: FsObj::TRANSFERRING|file.6: FsObj::TRANSFERRING"];
            after [label="file.1: FsObj::TRANSFERRING|file.2: FsObj::TRANSFERRING|file.3: FsObj::TRANSFERRING|file.4: FsObj::FAILED|file.5: FsObj::TRANSFERRING|file.6: FsObj::TRANSFERRING"];
            before -> after [];
       }
       @enddot
    -# A list is returned containing the inode numbers of these files where
       the previous operation was successful. Change all corresponding jobs
       to FsObj::TRANSFERRED or FsObj::MIGRATED depending of the migration
       phase. The following changed indicates that data transfer stopped
       before file file.5:
       @dot
       digraph step_1 {
            compound=true;
            fontname="courier";
            fontsize=11;
            rankdir=LR;
            node [shape=record, width=2, fontname="courier", fontsize=11, fillcolor=white, style=filled];
            before [label="file.1: FsObj::TRANSFERRING|file.2: FsObj::TRANSFERRING|file.3: FsObj::TRANSFERRING|file.4: FsObj::FAILED|file.5: FsObj::TRANSFERRING|file.6: FsObj::TRANSFERRING"];
            after [label="file.1: FsObj::TRANSFERRED|file.2: FsObj::TRANSFERRED|file.3: FsObj::TRANSFERRED|file.4: FsObj::FAILED|file.5: FsObj::TRANSFERRING|file.6: FsObj::TRANSFERRING"];
            before -> after [];
       }
       @enddot
    -# The remaining jobs (those where no corresponding inode numbers were
       in the list) have not been processed and need to be changed to the
       original state if these were still in FsObj::TRANSFERRING or
       FsObj::CHANGINGFSTATE state. Jobs that failed in the second step already
       have been marked as FsObj::FAILED. A reason for remaining jobs left
       over from the second step could be that a request with a higher
       priority (e.g. recall) required the same tape resource. This
       change is shown below for the remaining two files of the example:
       @dot
       digraph step_1 {
            compound=true;
            fontname="courier";
            fontsize=11;
            rankdir=LR;
            node [shape=record, width=2, fontname="courier", fontsize=11, fillcolor=white, style=filled];
            before [label="file.1: FsObj::TRANSFERRED|file.2: FsObj::TRANSFERRED|file.3: FsObj::TRANSFERRED|file.4: FsObj::FAILED|file.5: FsObj::TRANSFERRING|file.6: FsObj::TRANSFERRING"];
            after [label="file.1: FsObj::TRANSFERRED|file.2: FsObj::TRANSFERRED|file.3: FsObj::TRANSFERRED|file.4: FsObj::FAILED|file.5: FsObj::RESIDENT|file.6: FsObj::RESIDENT"];
            before -> after [];
       }
       @enddot

    If more than one job is processed the data transfer or migration state
    change operations can be performed in parallel. For data transfer each
    file needs to be written continuously on tape and therefore the writes
    are serialized. For this purpose two or more ThreadPool objects exists:

    - one ThreadPool object for migration state change: Server::wqs
    - for each LTFSDMDrive object one ThreadPool object: LTFSDMDrive::wqp
      to transfer data to tape.

    In the data transfer case the Migration::transferData method is executed
    and in case of changing the migration state it is the
    Migration::changeFileState method. Each of these methods operate on a
    single file.

    The following table provides a sequence of changes of different items
    that are changing during the migration of a resident file:

    &nbsp; | JOB_QUEUE <BR> %FsObj::file_state | attribute on disk: <BR> %FuseFS::mig_info::state_num <BR> and tape id | tape index<BR>synchronized | data on disk | data on tape
    ---|---|---|---|---|---
    1.  | RESIDENT       | no attributes           | - | + | -
    2.  | TRANSFERRING   | no attributes           | - | + | -
    3.  | TRANSFERRING   | IN_MIGRATION            | - | + | -
    4.  | TRANSFERRING   | IN_MIGRATION            | - | + | +
    5.  | TRANSFERRING   | IN_MIGRATION && tape id | - | + | +
    6.  | TRANSFERRED    | IN_MIGRATION && tape id | - | + | +
    7.  | TRANSFERRED    | IN_MIGRATION && tape id | + | + | +
    8.  | CHANGINGFSTATE | IN_MIGRATION && tape id | + | + | +
    9.  | CHANGINGFSTATE | STUBBING && tape id     | + | + | +
    10. | CHANGINGFSTATE | STUBBING && tape id     | + | - | +
    11. | CHANGINGFSTATE | MIGRATED && tape id     | + | - | +

    Each of these steps have a reason:

    -# This is the initial state when a resident file is newly added to the
       JOB_QUEUE table.
    -# The migration is split into three phases:
           -# transfer file data to a tape
           -# The the tape index stored on disk is written to the tape
           -# stub the file and mark it as migrated
           .
       In a first step all files that should be transferred to tape are
       marked according that operation to see which were left over in
       case the transfer has been suspended e.g. by a recall request on
       the same tape.
    -# The file attribute is changing to IN_MIGRATION. This intermediate
       state is to perform a proper cleanup in case the back end process
       terminates unexpectedly. See FuseFS::recoverState for recovery of
       a migration state.
    -# The file data is transferred to tape.
    -# The corresponding tape id is added to the attributes of the file.
    -# The state in the JOB_QUEUE table is changed to TRANSFERRED since
       that data transfer was successful.
    -# The the tape index stored on disk is written to the tape (migration
       phase 2 of 3).
    -# The last (third) migration phase is to stub the file and to mark it
       as migrated. It starts to change the state in the JOB_QUEUE table
       for all files that are targeted for this phase to CHANGINGFSTATE.
       Even the stubbing operation cannot be suspended it is to keep the
       the first and the third phase similar.
    -# The file attribute is changed to STUBBING. Like previously this
       intermediate state is to perform a proper cleanup in case the back
       end process terminates unexpectedly. See FuseFS::recoverState for
       recovery of a migration state.
    -# The file is truncated to zero.
    -# The attribute is changed to MIGRATED.


    ### Migration::transferData

    For data transfer the following steps are performed:

    -# In a loop the data is read from disk and written to tape.
    -# The FILE_PATH attribute is set on the data file on tape.
    -# A symbolic link is created by recreating the original
       full path on tape pointing to the corresponding data file.
    -# The status object @ref Status "mrStatus" gets updated
       for the output statistics.
    -# The tape is added to the attribute of the data file on tape.

    For data transfer each file needs to be written continuously on tape.
    Since the copy of data from disk to tape is performed in a loop by
    doing the reads and writes this loop is serialized by
    a std::mutex LTFSDMDrive::mtx.

    ### Migration::changeFileState

    For the change of the migration state (includes stubbing in the case that
    the migrated state is the target) the following steps are performed:

    -# The attributes on the disk file are changed accordingly.
    -# The file is truncated to zero size (only if the migrated state is the
       target).
    -# The status object @ref Status "mrStatus" gets updated
       for the output statistics.

    It is required that the attributes are changed before the file
    is truncated. It needs to be avoided that a file is truncated
    before it changes to migrated state. Otherwise: in an error case
    it could happen that the file is truncated but still in transferred
    state which indicates that the data locally is available.

 */

std::mutex Migration::pmigmtx;

ThreadPool<Migration, int, std::string, std::string, std::string, bool> Migration::swq(
        &Migration::execRequest, Const::MAX_STUBBING_THREADS, "stub2-wq");

FsObj::file_state Migration::checkState(std::string fileName, FsObj *fso)

{
    FsObj::file_state state;

    state = fso->getMigState();

    if (fso->getMigState() == FsObj::RESIDENT) {
        if (numReplica == 0) {
            MSG(LTFSDMS0065E, fileName);
            state = FsObj::FAILED;
        } else {
            needsTape = true;
        }
    } else {
        if (numReplica != 0) {
            FsObj::mig_tape_attr_t attr = fso->getAttribute();
            for (int i = 0; i < 3; i++) {
                if (std::string("").compare(attr.tapeInfo[i].tapeId) == 0) {
                    if (i == 0) {
                        MSG(LTFSDMS0066E, fileName);
                        state = FsObj::FAILED;
                        break;
                    } else {
                        break;
                    }
                }
                bool tapeFound = false;
                for (std::string pool : pools) {
                    std::lock_guard<std::recursive_mutex> lock(
                            LTFSDMInventory::mtx);
                    for (std::string cart : Server::conf.getPool(pool)) {
                        if (cart.compare(attr.tapeInfo[i].tapeId) == 0) {
                            tapeFound = true;
                            break;
                        }
                    }
                    if (tapeFound)
                        break;
                }
                if (!tapeFound) {
                    MSG(LTFSDMS0067E, fileName, attr.tapeInfo[i].tapeId);
                    state = FsObj::FAILED;
                    break;
                }
            }
        }
    }

    return state;
}

void Migration::addJob(std::string fileName)

{
    int replNum;
    struct stat statbuf;
    FsObj::file_state state;
    SQLStatement stmt;
    fuid_t fuid;

    try {
        FsObj fso(fileName);
        stat(fileName.c_str(), &statbuf);

        if (!S_ISREG(statbuf.st_mode)) {
            MSG(LTFSDMS0018E, fileName);
            return;
        }

        state = checkState(fileName, &fso);

        fuid = fso.getfuid();
        stmt(Migration::ADD_JOB) << DataBase::MIGRATION << fileName << reqNumber
                << targetState << statbuf.st_size << fuid.fsid_h << fuid.fsid_l
                << fuid.igen << fuid.inum << statbuf.st_mtim.tv_sec
                << statbuf.st_mtim.tv_nsec << time(NULL) << state;
    } catch (const std::exception& e) {
        MSG(LTFSDMS0077E, fileName);
        TRACE(Trace::error, e.what());
        stmt(Migration::ADD_JOB) << DataBase::MIGRATION << fileName << reqNumber
                << targetState << Const::UNSET << Const::UNSET << Const::UNSET
                << Const::UNSET << Const::UNSET << 0 << 0 << time(NULL)
                << FsObj::FAILED;
    }

    replNum = Const::UNSET;

    if (pools.size() == 0)
        pools.insert("");

    TRACE(Trace::normal, stmt.str());

    for (std::string pool : pools) {
        TRACE(Trace::full, stmt.str());
        try {
            replNum++;
            stmt.prepare();
            stmt.bind(1, replNum);
            stmt.bind(2, pool);
            stmt.step();
            stmt.finalize();
        } catch (const std::exception& e) {
            TRACE(Trace::error, e.what());
            MSG(LTFSDMS0028E, fileName);
        }
        TRACE(Trace::always, fileName, replNum, pool);
    }

    jobnum++;

    return;
}

void Migration::addRequest()

{
    int replNum;
    std::stringstream ssql;
    SQLStatement stmt;
    std::stringstream thrdinfo;

    for (std::string pool : pools)
        TRACE(Trace::normal, pool);

    {
        std::lock_guard<std::mutex> updlock(Scheduler::updmtx);
        Scheduler::updReq[reqNumber] = false;
    }

    replNum = Const::UNSET;

    for (std::string pool : pools) {
        replNum++;
        std::unique_lock<std::mutex> lock(Scheduler::mtx);

        stmt(Migration::ADD_REQUEST) << DataBase::MIGRATION << reqNumber
                << targetState << numReplica << replNum << pool << time(NULL)
                << (needsTape ? DataBase::REQ_NEW : DataBase::REQ_INPROGRESS);

        TRACE(Trace::normal, stmt.str());

        stmt.doall();

        TRACE(Trace::always, needsTape, reqNumber, pool);

        if (needsTape) {
            Scheduler::cond.notify_one();
        } else {
            swq.enqueue(reqNumber,
                    Migration(getpid(), reqNumber, { }, numReplica,
                            targetState), replNum, "", pool, "", needsTape);
        }
    }

    swq.waitCompletion(reqNumber);
}

unsigned long Migration::transferData(std::string tapeId, std::string driveId,
        long secs, long nsecs, Migration::mig_info_t mig_info,
        std::shared_ptr<std::list<unsigned long>> inumList,
        std::shared_ptr<bool> suspended)

{
    struct stat statbuf, statbuf_changed;
    std::string tapeName;
    char buffer[Const::READ_BUFFER_SIZE];
    long rsize;
    long wsize;
    int fd = -1;
    long offset = 0;
    bool failed = false;

    try {
        FsObj source(mig_info.fileName);

        TRACE(Trace::always, mig_info.fileName);

        tapeName = Server::getTapeName(&source, tapeId);

        Server::createDataDir(tapeId);

        fd = Server::openTapeRetry(tapeId, tapeName.c_str(),
                O_RDWR | O_CREAT | O_TRUNC | O_CLOEXEC);

        if (fd == -1) {
            TRACE(Trace::error, errno);
            MSG(LTFSDMS0021E, tapeName.c_str());
            THROW(Error::GENERAL_ERROR, tapeName, errno);
        }

        std::unique_lock<FsObj> fsolock(source);

        source.preparePremigration();

        fsolock.unlock();

        if (stat(mig_info.fileName.c_str(), &statbuf) == -1) {
            TRACE(Trace::error, errno);
            MSG(LTFSDMS0040E, mig_info.fileName);
            THROW(Error::GENERAL_ERROR, mig_info.fileName, errno);
        }
        if (statbuf.st_mtim.tv_sec != secs
                || statbuf.st_mtim.tv_nsec != nsecs) {
            TRACE(Trace::error, statbuf.st_mtim.tv_sec, secs,
                    statbuf.st_mtim.tv_nsec, nsecs);
            MSG(LTFSDMS0041W, mig_info.fileName);
            THROW(Error::GENERAL_ERROR, mig_info.fileName);
        }

        {
            std::lock_guard<std::mutex> writelock(
                    *inventory->getDrive(driveId)->mtx);

            if (inventory->getDrive(driveId)->getToUnblock()
                    < DataBase::MIGRATION) {
                TRACE(Trace::always, mig_info.fileName, tapeId);
                std::lock_guard<std::mutex> lock(Migration::pmigmtx);
                *suspended = true;
                THROW(Error::OK);
            }

            while (offset < statbuf.st_size) {
                if (Server::forcedTerminate)
                    THROW(Error::OK);

                rsize = source.read(offset,
                        statbuf.st_size - offset > Const::READ_BUFFER_SIZE ?
                                Const::READ_BUFFER_SIZE :
                                statbuf.st_size - offset, buffer);
                if (rsize == -1) {
                    TRACE(Trace::error, errno);
                    MSG(LTFSDMS0023E, mig_info.fileName);
                    THROW(Error::GENERAL_ERROR, errno, mig_info.fileName);
                }

                wsize = write(fd, buffer, rsize);

                if (wsize != rsize) {
                    TRACE(Trace::error, errno, wsize, rsize);
                    MSG(LTFSDMS0022E, tapeName.c_str());
                    THROW(Error::GENERAL_ERROR, mig_info.fileName, wsize,
                            rsize);
                }

                offset += rsize;
                if (stat(mig_info.fileName.c_str(), &statbuf_changed) == -1) {
                    TRACE(Trace::error, errno);
                    MSG(LTFSDMS0040E, mig_info.fileName);
                    THROW(Error::GENERAL_ERROR, mig_info.fileName, errno);
                }

                if (statbuf_changed.st_mtim.tv_sec != secs
                        || statbuf_changed.st_mtim.tv_nsec != nsecs) {
                    TRACE(Trace::error, statbuf_changed.st_mtim.tv_sec, secs,
                            statbuf_changed.st_mtim.tv_nsec, nsecs);
                    MSG(LTFSDMS0041W, mig_info.fileName);
                    THROW(Error::GENERAL_ERROR, mig_info.fileName);
                }
            }
        }

        if (fsetxattr(fd, Const::LTFS_ATTR.c_str(), mig_info.fileName.c_str(),
                mig_info.fileName.length(), 0) == -1) {
            TRACE(Trace::error, errno);
            MSG(LTFSDMS0025E, Const::LTFS_ATTR, tapeName);
            THROW(Error::GENERAL_ERROR, mig_info.fileName, errno);
        }

        Server::createLink(tapeId, mig_info.fileName, tapeName);

        mrStatus.updateSuccess(mig_info.reqNumber, mig_info.fromState,
                mig_info.toState);

        source.addTapeAttr(tapeId, Server::getStartBlock(tapeName, fd));

        std::lock_guard<std::mutex> lock(Migration::pmigmtx);
        inumList->push_back(mig_info.inum);
    } catch (const LTFSDMException& e) {
        TRACE(Trace::error, e.what());
        if (e.getError() != Error::OK)
            failed = true;
    } catch (const std::exception& e) {
        TRACE(Trace::error, e.what());
        failed = true;
    }

    if (failed == true) {
        TRACE(Trace::error, mig_info.fileName);
        MSG(LTFSDMS0050E, mig_info.fileName);
        mrStatus.updateFailed(mig_info.reqNumber, mig_info.fromState);

        SQLStatement stmt = SQLStatement(Migration::FAIL_PREMIGRATION)
                << FsObj::FAILED << mig_info.reqNumber << mig_info.fileName
                << mig_info.replNum;

        stmt.doall();
    }

    if (fd != -1)
        close(fd);

    return statbuf.st_size;
}

void Migration::changeFileState(Migration::mig_info_t mig_info,
        std::shared_ptr<std::list<unsigned long>> inumList,
        FsObj::file_state toState)

{
    try {
        FsObj source(mig_info.fileName);
        FsObj::mig_tape_attr_t attr;

        TRACE(Trace::always, mig_info.fileName);

        std::lock_guard<FsObj> fsolock(source);
        attr = source.getAttribute();
        if ((source.getMigState() != FsObj::MIGRATED
                && source.getMigState() != FsObj::PREMIGRATED)
                && (mig_info.numRepl == 0 || attr.copies == mig_info.numRepl)) {
            if (toState == FsObj::MIGRATED) {
                source.prepareStubbing();
                source.stub();
            } else {
                source.finishPremigration();
            }
            TRACE(Trace::full, mig_info.fileName);
        } else {
            TRACE(Trace::always, mig_info.fileName, source.getMigState());
            return;
        }

        std::lock_guard<std::mutex> lock(Migration::pmigmtx);
        inumList->push_back(mig_info.inum);
    } catch (const std::exception& e) {
        TRACE(Trace::error, e.what());
        MSG(LTFSDMS0089E, mig_info.fileName);

        for (int i = 0; i < mig_info.numRepl; i++)
            mrStatus.updateFailed(mig_info.reqNumber, mig_info.fromState);

        SQLStatement stmt = SQLStatement(Migration::FAIL_STUBBING)
                << FsObj::FAILED << mig_info.reqNumber << mig_info.fileName;

        stmt.doall();
        return;
    }

    for (int i = 0; i < (mig_info.numRepl ? mig_info.numRepl : 1); i++)
        mrStatus.updateSuccess(mig_info.reqNumber, mig_info.fromState,
                mig_info.toState);
}

Migration::req_return_t Migration::processFiles(int replNum, std::string tapeId,
        FsObj::file_state fromState, FsObj::file_state toState)

{
    SQLStatement stmt;
    std::string fileName;
    Migration::req_return_t retval = (Migration::req_return_t ) { false, false };
    time_t start;
    long secs;
    long nsecs;
    unsigned long inum;
    time_t steptime;
    std::shared_ptr<std::list<unsigned long>> inumList = std::make_shared<
            std::list<unsigned long>>();
    std::shared_ptr<bool> suspended = std::make_shared<bool>(false);
    unsigned long freeSpace = 0;
    int num_found = 0;
    int total = 0;
    FsObj::file_state newState;
    std::shared_ptr<LTFSDMDrive> drive = nullptr;

    TRACE(Trace::always, reqNumber);

    {
        std::lock_guard<std::mutex> lock(Scheduler::updmtx);
        Scheduler::updReq[reqNumber] = true;
        Scheduler::updcond.notify_all();
    }

    if (toState == FsObj::TRANSFERRED) {
        for (std::shared_ptr<LTFSDMDrive> d : inventory->getDrives()) {
            if (d->get_le()->get_slot()
                    == inventory->getCartridge(tapeId)->get_le()->get_slot()) {
                drive = d;
                break;
            }
        }
        assert(drive != nullptr);
    }

    newState = (
            (toState == FsObj::TRANSFERRED) ?
                    FsObj::TRANSFERRING : FsObj::CHANGINGFSTATE);

    if (toState == FsObj::TRANSFERRED) {
        freeSpace =
                1024 * 1024
                        * inventory->getCartridge(tapeId)->get_le()->get_remaining_cap();
        stmt(Migration::SET_TRANSFERRING) << newState << tapeId << reqNumber
                << fromState << replNum << (unsigned long) &freeSpace
                << (unsigned long) &num_found << (unsigned long) &total;
    } else {
        stmt(Migration::SET_CHANGE_STATE) << newState << reqNumber << fromState
                << tapeId << replNum;
    }
    TRACE(Trace::normal, stmt.str());
    steptime = time(NULL);
    stmt.doall();

    TRACE(Trace::always, time(NULL) - steptime, num_found, total);
    if (total > num_found)
        retval.remaining = true;

    stmt(Migration::SELECT_JOBS) << reqNumber << newState << tapeId;
    TRACE(Trace::normal, stmt.str());
    stmt.prepare();
    start = time(NULL);
    while (stmt.step(&fileName, &secs, &nsecs, &inum)) {
        if (Server::terminate == true)
            break;

        try {
            Migration::mig_info_t mig_info = { fileName, reqNumber, numReplica,
                    replNum, inum, "", fromState, toState };

            TRACE(Trace::always, fileName, reqNumber);

            if (toState == FsObj::TRANSFERRED) {
                if (drive->getToUnblock() < DataBase::MIGRATION) {
                    retval.suspended = true;
                    break;
                }
                TRACE(Trace::full, secs, nsecs);
                drive->wqp->enqueue(reqNumber, tapeId,
                        drive->get_le()->GetObjectID(), secs, nsecs, mig_info,
                        inumList, suspended);
            } else {
                Server::wqs->enqueue(reqNumber, mig_info, inumList, toState);
            }
        } catch (const std::exception& e) {
            TRACE(Trace::error, e.what());
            continue;
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

    if (toState == FsObj::TRANSFERRED) {
        drive->wqp->waitCompletion(reqNumber);
    } else {
        Server::wqs->waitCompletion(reqNumber);
    }

    if (*suspended == true)
        retval.suspended = true;

    stmt(Migration::SET_JOB_SUCCESS) << toState << reqNumber << newState
            << tapeId << genInumString(*inumList);
    TRACE(Trace::normal, stmt.str());
    steptime = time(NULL);
    stmt.doall();
    TRACE(Trace::always, time(NULL) - steptime);

    stmt(Migration::RESET_JOB_STATE) << fromState << reqNumber << newState
            << tapeId;
    TRACE(Trace::normal, stmt.str());
    steptime = time(NULL);
    stmt.doall();
    TRACE(Trace::always, time(NULL) - steptime);

    return retval;
}

/**
 *
 * @param replNum
 * @param driveId
 * @param pool
 * @param tapeId
 * @param needsTape
 *
 * @bug needsTape vs. Migration::needsTape
 *
 */
void Migration::execRequest(int replNum, std::string driveId, std::string pool,
        std::string tapeId,
        bool needsTape)

{
    TRACE(Trace::full, __PRETTY_FUNCTION__);

    SQLStatement stmt;
    Migration::req_return_t retval = (Migration::req_return_t ) { false, false };
    bool failed = false;
    int rc;

    mrStatus.add(reqNumber);

    TRACE(Trace::always, reqNumber, needsTape, tapeId);

    if (needsTape) {
        retval = processFiles(replNum, tapeId, FsObj::RESIDENT,
                FsObj::TRANSFERRED);

        try {
            if ((rc = inventory->getCartridge(tapeId)->get_le()->Sync()) != 0)
                THROW(Error::GENERAL_ERROR, rc);
        } catch (const std::exception& e) {
            TRACE(Trace::error, errno);
            MSG(LTFSDMS0024E, tapeId);

            stmt(Migration::FAIL_PREMIGRATED) << FsObj::FAILED << reqNumber
                    << FsObj::PREMIGRATED << tapeId << replNum;

            TRACE(Trace::error, stmt.str());

            stmt.doall();

            std::unique_lock<std::mutex> lock(Scheduler::updmtx);
            TRACE(Trace::error, reqNumber);
            Scheduler::updReq[reqNumber] = true;
            Scheduler::updcond.notify_all();

            failed = true;
        }

        inventory->update(inventory->getCartridge(tapeId));

        std::lock_guard<std::recursive_mutex> lock(LTFSDMInventory::mtx);
        if (inventory->getCartridge(tapeId)->getState()
                == LTFSDMCartridge::TAPE_INUSE)
            inventory->getCartridge(tapeId)->setState(
                    LTFSDMCartridge::TAPE_MOUNTED);

        inventory->getDrive(driveId)->setFree();
        inventory->getDrive(driveId)->clearToUnblock();

        Scheduler::cond.notify_one();
    }

    if (!failed) {
        if (targetState == LTFSDmProtocol::LTFSDmMigRequest::MIGRATED) {
            if (needsTape)
                processFiles(replNum, tapeId, FsObj::TRANSFERRED,
                        FsObj::MIGRATED);
            else
                processFiles(replNum, tapeId, FsObj::PREMIGRATED,
                        FsObj::MIGRATED);
        } else {
            if (needsTape)
                processFiles(replNum, tapeId, FsObj::TRANSFERRED,
                        FsObj::PREMIGRATED);
        }
    }

    std::unique_lock<std::mutex> updlock(Scheduler::updmtx);

    if (retval.suspended)
        stmt(Migration::UPDATE_REQUEST) << DataBase::REQ_NEW << reqNumber
                << replNum;
    else if (retval.remaining)
        stmt(Migration::UPDATE_REQUEST_RESET_TAPE) << DataBase::REQ_NEW
                << reqNumber << replNum;
    else
        stmt(Migration::UPDATE_REQUEST) << DataBase::REQ_COMPLETED << reqNumber
                << replNum;

    TRACE(Trace::normal, stmt.str());

    stmt.doall();

    Scheduler::updReq[reqNumber] = true;
    Scheduler::updcond.notify_all();
}
