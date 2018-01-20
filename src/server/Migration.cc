#include "ServerIncludes.h"

/** @page migration Migration

    # Migration

    The migration processing happens within two phases:

    1. The backend receives a migration message which is further processed
       within the MessageParser::migrationMessage method. During this
       processing migration jobs and migration requests are added to
       the internal queues.
    2. The Scheduler identifies a migration request to get scheduled.\n
       The migration happens within the following sequence:
       - The corresponding data is written to the selected tape.
       - The tape index is synchronized.
       - The corresponding files on disk are stubbed (only for migration
         state as target).


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
                mig_exec [label="{ <write_to_tape> write to tape\n(Migration::preMigrate)|<sync_index> sync index|<stub> stub files\n(Migration::stub)}"];
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

    If the premigration state is chosen as the target migration state the third
    item - the stubbing phase - of the second step is skipped.

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
    Scheduler::run:
    - if a migration request is ready do be scheduled:
        - update record in request queue to mark it as DataBase::REQ_INPROGRESS
        - Migration::execRequest
            - add status: @ref Status::add "mrStatus.add"
            - if @ref Migration::needsTape "needsTape" is true:
                - Migration::processFiles: premigrate all files according this request
                - synchronize tape index
                - release tape for further operations since for stubbing files there is nothing written to tape
            - Migration::processFiles: stub all files according this request
            - update record in request queue to mark it as DataBase::REQ_COMPLETED

    </TT>

    In Migration::execRequest adding an entry to the @ref mrStatus object is
    necessary for the client that initiated the request to receive progress
    information.

    ### Migration::processFiles

    The Migration::processFiles method is called twice first to premigrate
    files and a second time to stub them if necessary. If all files
    to be processed are already premigrated there is no need to mount a
    cartridge. In this case the premigration step is skipped. If the
    target state is LTFSDmProtocol::LTFSDmMigRequest::PREMIGRATED the
    step to stub files is skipped.

    The Migration::processFiles method in general perform the following
    steps:

    -# All corresponding jobs are changed to FsObj::PREMIGRATING or FsObj::STUBBING
       depending if it is called for premigration or stubbing. The following
       example shows this change for a primigration phase of six files:
       @dot
       digraph step_1 {
            compound=true;
            fontname="courier";
            fontsize=11;
            rankdir=LR;
            node [shape=record, width=2, fontname="courier", fontsize=11, fillcolor=white, style=filled];
            before [label="file.1: FsObj::RESIDENT|file.2: FsObj::RESIDENT|file.3: FsObj::RESIDENT|file.4: FsObj::RESIDENT|file.5: FsObj::RESIDENT|file.6: FsObj::RESIDENT"];
            after [label="file.1: FsObj::PREMIGRATING|file.2: FsObj::PREMIGRATING|file.3: FsObj::PREMIGRATING|file.4: FsObj::PREMIGRATING|file.5: FsObj::PREMIGRATING|file.6: FsObj::PREMIGRATING"];
            before -> after [];
       }
       @enddot
    -# Process all these jobs in FsObj::PREMIGRATING or FsObj::STUBBING state
       which results in the premigration or stubbing of all corresponding files.
       The following change indicates that the premigration of file file.4 failed:
       @dot
       digraph step_1 {
            compound=true;
            fontname="courier";
            fontsize=11;
            rankdir=LR;
            node [shape=record, width=2, fontname="courier", fontsize=11, fillcolor=white, style=filled];
            before [label="file.1: FsObj::PREMIGRATING|file.2: FsObj::PREMIGRATING|file.3: FsObj::PREMIGRATING|file.4: FsObj::PREMIGRATING|file.5: FsObj::PREMIGRATING|file.6: FsObj::PREMIGRATING"];
            after [label="file.1: FsObj::PREMIGRATING|file.2: FsObj::PREMIGRATING|file.3: FsObj::PREMIGRATING|file.4: FsObj::FAILED|file.5: FsObj::PREMIGRATING|file.6: FsObj::PREMIGRATING"];
            before -> after [];
       }
       @enddot
    -# A list is returned containing the inode numbers of these files where
       the previous operation was successful. Change all corresponding jobs
       to FsObj::PREMIGRATED or FsObj::MIGRATED depending of the migration
       phase. The following changed indicates that premigration stopped
       before file file.5:
       @dot
       digraph step_1 {
            compound=true;
            fontname="courier";
            fontsize=11;
            rankdir=LR;
            node [shape=record, width=2, fontname="courier", fontsize=11, fillcolor=white, style=filled];
            before [label="file.1: FsObj::PREMIGRATING|file.2: FsObj::PREMIGRATING|file.3: FsObj::PREMIGRATING|file.4: FsObj::FAILED|file.5: FsObj::PREMIGRATING|file.6: FsObj::PREMIGRATING"];
            after [label="file.1: FsObj::PREMIGRATED|file.2: FsObj::PREMIGRATED|file.3: FsObj::PREMIGRATED|file.4: FsObj::FAILED|file.5: FsObj::PREMIGRATING|file.6: FsObj::PREMIGRATING"];
            before -> after [];
       }
       @enddot
    -# The remaining jobs (those where no corresponding inode numbers were
       in the list) have not been processed and need to be changed to the
       original state if these were still in FsObj::PREMIGRATING or
       FsObj::STUBBING state. Jobs that failed in the second step already
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
            before [label="file.1: FsObj::PREMIGRATED|file.2: FsObj::PREMIGRATED|file.3: FsObj::PREMIGRATED|file.4: FsObj::FAILED|file.5: FsObj::PREMIGRATING|file.6: FsObj::PREMIGRATING"];
            after [label="file.1: FsObj::PREMIGRATED|file.2: FsObj::PREMIGRATED|file.3: FsObj::PREMIGRATED|file.4: FsObj::FAILED|file.5: FsObj::RESIDENT|file.6: FsObj::RESIDENT"];
            before -> after [];
       }
       @enddot

    If more than one job is processed the premigration
    or stubbing operations can be performed in parallel. For premigration each
    file needs to be written continuously on tape and therefore the writes
    are serialized . For this purpose two or more ThreadPool objects exists:

    - one ThreadPool object for stubbing: Server::wqs
    - for each LTFSDMDrive object one ThreadPool object: LTFSDMDrive::wqp

    In the premigration case the Migration::preMigrate method is executed
    and the stubbing case it is the Migration::stub method. Each of these
    methods operate on a single file.

    ### Migration::preMigrate

    For premigration the following steps are performed:

    -# In a loop the data is read from disk and written to tape.
    -# The FILE_PATH attribute is set on the data file on tape.
    -# A symbolic link is created by recreating the original
       full path on tape pointing to the corresponding data file.
    -# The status object @ref Status "mrStatus" gets updated
       for the output statistics.
    -# The attributes on the disk file are updated.

    For premigration each file needs to be written continuously on tape.
    Since the copy of data from disk to tape is performed in a loop by
    doing the reads and writes this loop is serialized by
    a std::mutex LTFSDMDrive::mtx.

    ### Migration::stub

    For stubbing the following steps are performed:

    -# The attributes on the disk file are changed accordingly.
    -# The file is truncated to zero size.
    -# The status object @ref Status "mrStatus" gets updated
       for the output statistics.

    It is required that the attributes are changed before the file
    is truncated. It needs to be avoided that a file is truncated
    before it changes to migrated state. Otherwise: in an error case
    it could happen that the file is truncated but still in premigrated
    state. A recall of this "premigrated" file just changes the file
    state from premigrated to resident assuming the data is still on disk.

 */

std::mutex Migration::pmigmtx;

ThreadPool<Migration, int, std::string, std::string, bool> Migration::swq(
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
            FsObj::mig_attr_t attr = fso->getAttribute();
            for (int i = 0; i < 3; i++) {
                if (std::string("").compare(attr.tapeId[i]) == 0) {
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
                        if (cart.compare(attr.tapeId[i]) == 0) {
                            tapeFound = true;
                            break;
                        }
                    }
                    if (tapeFound)
                        break;
                }
                if (!tapeFound) {
                    MSG(LTFSDMS0067E, fileName, attr.tapeId[i]);
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
        TRACE(Trace::error, e.what());
        stmt(Migration::ADD_JOB) << DataBase::MIGRATION << fileName << reqNumber
                << targetState << Const::UNSET << Const::UNSET << Const::UNSET
                << Const::UNSET << Const::UNSET << Const::UNSET << time(NULL)
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
                            targetState), replNum, pool, "", needsTape);
        }
    }

    swq.waitCompletion(reqNumber);
}

unsigned long Migration::preMigrate(std::string tapeId, std::string driveId,
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
    FsObj::mig_attr_t attr;
    bool failed = false;

    try {
        FsObj source(mig_info.fileName);

        TRACE(Trace::always, mig_info.fileName);

        tapeName = Server::getTapeName(&source, tapeId);

        Server::createDataDir(tapeId);

        fd = open(tapeName.c_str(), O_RDWR | O_CREAT | O_TRUNC | O_CLOEXEC);

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

        fsolock.lock();
        attr = source.getAttribute();
        memset(attr.tapeId[attr.copies], 0, Const::tapeIdLength + 1);
        strncpy(attr.tapeId[attr.copies], tapeId.c_str(), Const::tapeIdLength);
        attr.copies++;
        source.addAttribute(attr);
        if (attr.copies == mig_info.numRepl)
            source.finishPremigration();

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

void Migration::stub(Migration::mig_info_t mig_info,
        std::shared_ptr<std::list<unsigned long>> inumList)

{
    try {
        FsObj source(mig_info.fileName);
        FsObj::mig_attr_t attr;

        TRACE(Trace::always, mig_info.fileName);

        std::lock_guard<FsObj> fsolock(source);
        attr = source.getAttribute();
        if ((source.getMigState() != FsObj::MIGRATED)
                && (mig_info.numRepl == 0 || attr.copies == mig_info.numRepl)) {
            source.prepareStubbing();
            source.stub();
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
    FsObj::file_state state;
    std::shared_ptr<LTFSDMDrive> drive = nullptr;

    TRACE(Trace::always, reqNumber);

    {
        std::lock_guard<std::mutex> lock(Scheduler::updmtx);
        Scheduler::updReq[reqNumber] = true;
        Scheduler::updcond.notify_all();
    }

    if (toState == FsObj::PREMIGRATED) {
        for (std::shared_ptr<LTFSDMDrive> d : inventory->getDrives()) {
            if (d->get_slot() == inventory->getCartridge(tapeId)->get_slot()) {
                drive = d;
                break;
            }
        }
        assert(drive != nullptr);
    }

    state = (
            (toState == FsObj::PREMIGRATED) ?
                    FsObj::PREMIGRATING : FsObj::STUBBING);

    if (toState == FsObj::PREMIGRATED) {
        freeSpace = 1024 * 1024
                * inventory->getCartridge(tapeId)->get_remaining_cap();
        stmt(Migration::SET_PREMIGRATING) << state << tapeId << reqNumber
                << fromState << replNum << (unsigned long) &freeSpace
                << (unsigned long) &num_found << (unsigned long) &total;
    } else {
        stmt(Migration::SET_STUBBING) << state << reqNumber << fromState
                << tapeId << replNum;
    }
    TRACE(Trace::normal, stmt.str());
    steptime = time(NULL);
    stmt.doall();

    TRACE(Trace::always, time(NULL) - steptime, num_found, total);
    if (total > num_found)
        retval.remaining = true;

    stmt(Migration::SELECT_JOBS) << reqNumber << state << tapeId;
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

            if (toState == FsObj::PREMIGRATED) {
                if (drive->getToUnblock() < DataBase::MIGRATION) {
                    retval.suspended = true;
                    break;
                }
                TRACE(Trace::full, secs, nsecs);
                drive->wqp->enqueue(reqNumber, tapeId, drive->GetObjectID(),
                        secs, nsecs, mig_info, inumList, suspended);
            } else {
                Server::wqs->enqueue(reqNumber, mig_info, inumList);
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

    if (toState == FsObj::PREMIGRATED) {
        drive->wqp->waitCompletion(reqNumber);
    } else {
        Server::wqs->waitCompletion(reqNumber);
    }

    if (*suspended == true)
        retval.suspended = true;

    stmt(Migration::SET_JOB_SUCCESS) << toState << reqNumber << state << tapeId
            << genInumString(*inumList);
    TRACE(Trace::normal, stmt.str());
    steptime = time(NULL);
    stmt.doall();
    TRACE(Trace::always, time(NULL) - steptime);

    stmt(Migration::RESET_JOB_STATE) << fromState << reqNumber << state
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
 * @param pool
 * @param tapeId
 * @param needsTape
 *
 * @bug needsTape vs. Migration::needsTape
 *
 */
void Migration::execRequest(int replNum, std::string pool, std::string tapeId,
bool needsTape)

{
    TRACE(Trace::full, __PRETTY_FUNCTION__);

    SQLStatement stmt;
    std::stringstream tapePath;
    Migration::req_return_t retval = (Migration::req_return_t ) { false, false };
    bool failed = false;

    mrStatus.add(reqNumber);

    TRACE(Trace::always, reqNumber, needsTape, tapeId);

    if (needsTape) {
        retval = processFiles(replNum, tapeId, FsObj::RESIDENT,
                FsObj::PREMIGRATED);

        tapePath << inventory->getMountPoint() << "/" << tapeId;

        if (setxattr(tapePath.str().c_str(), Const::LTFS_SYNC_ATTR.c_str(),
                Const::LTFS_SYNC_VAL.c_str(), Const::LTFS_SYNC_VAL.length(), 0)
                == -1) {
            if ( errno != ENODATA) {
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
        }

        inventory->update(inventory->getCartridge(tapeId));

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
        Scheduler::cond.notify_one();
    }

    if (!failed && targetState != LTFSDmProtocol::LTFSDmMigRequest::PREMIGRATED)
        processFiles(replNum, tapeId, FsObj::PREMIGRATED, FsObj::MIGRATED);

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
