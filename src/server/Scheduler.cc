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

/** @page scheduler Scheduler

    # Scheduler

    The Scheduler main method Scheduler::run is started by the Server::run
    method and continuously running as an additional thread. For an overview
    about all threads that are started within the backend have a look at
    @ref server_code. Within the Server::run routine a loop is waiting on a
    condition of either a new request has been added or a resource became free.
    If there is a request that has not been scheduled so far and a corresponding
    resource is became this request can be scheduled. Therefore there can be
    two possibilities a request get scheduled:

    - A new request has been added an a cartridge and drive resource already is
      available.
    - A request has been previously added but there was no free drive and
      cartridge resource available at that time. Now, a corresponding resource
      became free.

    Within the outer while loop of Scheduler::runthe condition Scheduler::cond
    is waiting for a lock on the Scheduler::mtx mutex.

    The scheduler also initiates mount and unmounts of cartridges. E.g. if there
    is a new request to migrate data but all available drives are empty the
    scheduler initiates a tape mount for a corresponding cartridge.
    Therefore a notification on the Scheduler::cond condition is done in the
    following cases:

    - a new request has been added
    - a request has been completed: the corresponding tape and drive resource
      is available thereafter
    - a tape mount is completed (see @ref LTFSDMInventory::mount): drive and
      cartridge can be used to schedule a request
    - a tape unmount is completed (see @ref LTFSDMInventory::unmount): drive
      can be used to mount a cartridge

    After that Scheduler::resAvail checks if there is a resource available
    to schedule a request or to mount and unmount cartridges. For recall a
    specific cartridge needs to be considered (Scheduler::tapeResAvail). For
    migration it needs to be a cartridge from a corresponding tape storage pool
    where at least one file will fit on it (Scheduler::poolResAvail).

    @dot
    digraph scheduler {
        compound=true;
        fontname="courier";
        fontsize=11;
        labeljust=l;
        node [shape=record, width=2, fontname="courier", fontsize=11, fillcolor=white, style=filled];
        wait [label="wait for a new request or a free resource"];
        subgraph cluster_res_avail {
            res_avail [fontname="courier bold", fontcolor=dodgerblue4, label="Scheduler::resAvail", URL="@ref Scheduler::resAvail"];
            tape_res_avail [fontname="courier bold", fontcolor=dodgerblue4, label="Scheduler::tapeResAvail", URL="@ref Scheduler::tapeResAvail"];
            pool_res_avail [fontname="courier bold", fontcolor=dodgerblue4, label="Scheduler::poolResAvail", URL="@ref Scheduler::poolResAvail"];
        }
        schedule_mig [label="schedule migration"];
        schedule_rec [label="{<srec> schedule selective recall|<trec> schedule transparent recall}"];
        wait -> res_avail[lhead=cluster_res_avail];
        res_avail -> pool_res_avail [fontsize=8, label="if migration"];
        res_avail -> tape_res_avail[];
        tape_res_avail -> schedule_rec [ltail=cluster_res_avail, fontsize=8, label="if resource found"];
        pool_res_avail -> schedule_mig [ltail=cluster_res_avail, fontsize=8, label="if resource found"];
    }
    @enddot

    ## Scheduler::tapeResAvail

    A tape resource is checked for availability in the following way (return
    statements are performed in respect to the condition):

    -# If the corresponding cartridge is moving: <b>return false</b>.
    -# If corresponding cartridge is in use (by another request) try to unblock
       the cartridge. If e.g. the cartridge is used for migration recall
       requests have a higher priority and can led the migration request to
       suspend processing. Since there is no free resource: <b>return false</b>.
    -# Else if the corresponding cartridge is mounted (but not in use) it
       can be used for the current request: <b>return true</b>.
    -# Thereafter it is checked for free (not in use) drives. If there is a
       drive that has cartridge mounted that is not in use unmount this
       cartridge: <b>return false</b>.
    -# Next it is checked if a operation with a lower priority can be
       suspended. If an operation already has been suspended
       (LTFSDMCartridge::isRequested is true): <b>return false</b>.
    -# Now try to suspend an operation.
    -# <b>return false</b>


    ## Scheduler::poolResAvail

    A tape storage pool is checked for availability in the following way
    (return statements are performed in respect to the condition):

    -# If a cartridge of the specified tape storage pool is mounted but not
       in use and the remaining space is larger than the smallest file to
       migrate: <b>return true</b>.
    -# If there is no cartridge that is not mounted there is no need to look
       for a cartridge from another pool to unmount: <b>return false</b>.
    -# Check if there is an empty drive to mount a tape which is part of the
       specified pool. If this is the case: <b>return false</b>.
    -# Check if a for the current request there is a tape mount/unmount already
       in progress. If this is the case: <b>return false</b>.
    -# Thereafter it is checked if there is a cartridge from another pool
       that is mounted but not in use. Unmount this cartridge and:
       <b>return false</b>.
    -# <b>return false</b>

    ## Schedule request

    If Scheduler::resAvail is true a request can be scheduled. Depending on
    the operation type  a new thread is created (Scheduler::subs,
    SubServer::enqueue) to execute:

    operation type | executed method
    ---|---
    DataBase::MIGRATION | Migration::execRequest
    DataBase::SELRECALL | SelRecall::execRequest
    DataBase::TRARECALL | TransRecall::execRequest

 */

std::mutex Scheduler::mtx;
std::condition_variable Scheduler::cond;
std::mutex Scheduler::updmtx;
std::condition_variable Scheduler::updcond;
std::map<int, std::atomic<bool>> Scheduler::updReq;

void Scheduler::makeUse(std::string driveId, std::string tapeId)

{
    std::shared_ptr<LTFSDMCartridge> cart = inventory->getCartridge(tapeId);
    std::shared_ptr<LTFSDMDrive> drive = inventory->getDrive(driveId);

    TRACE(Trace::always, driveId, tapeId);
    drive->setBusy();
    cart->setState(LTFSDMCartridge::TAPE_INUSE);
}

void Scheduler::moveTape(std::string driveId, std::string tapeId, Mount::operation op)

{
    std::shared_ptr<LTFSDMCartridge> cart = inventory->getCartridge(tapeId);
    std::shared_ptr<LTFSDMDrive> drive = inventory->getDrive(driveId);
    std::string opstr(op == Mount::MOUNT ? "mnt:" : "umn:");

    TRACE(Trace::always, driveId, tapeId);
    Scheduler::makeUse(driveId, tapeId);
    drive->setUnmountReqNum(reqNum);
    subs.enqueue(std::string(opstr) + tapeId,
            &Mount::addRequest,
            Mount(driveId, tapeId, op));
}

bool Scheduler::poolResAvail(unsigned long minFileSize)

{
    bool found;
    bool unmountedExists = false;

    assert(pool.compare("") != 0);

    for (std::string cartname : Server::conf.getPool(pool)) {
        std::shared_ptr<LTFSDMCartridge> cart;
        if ((cart = inventory->getCartridge(cartname)) == nullptr) {
            MSG(LTFSDMX0034E, cartname);
            Server::conf.poolRemove(pool, cartname);
        }
        if (cart->getState() == LTFSDMCartridge::TAPE_MOUNTED) {
            tapeId = cart->get_le()->GetObjectID();
            found = false;
            for (std::shared_ptr<LTFSDMDrive> drive : inventory->getDrives()) {
                if (drive->get_le()->get_slot() == cart->get_le()->get_slot()
                        && 1024 * 1024 * cart->get_le()->get_remaining_cap()
                                >= minFileSize) {
                    assert(drive->isBusy() == false);
                    TRACE(Trace::always, drive->get_le()->GetObjectID());
                    driveId = drive->get_le()->GetObjectID();
                    Scheduler::makeUse(driveId, tapeId);
                    found = true;
                    break;
                }
            }
            assert(
                    found == true || 1024*1024*cart->get_le()->get_remaining_cap() < minFileSize);
            if (found == true)
                return true;
        } else if (cart->getState() == LTFSDMCartridge::TAPE_UNMOUNTED)
            unmountedExists = true;
    }

    if (unmountedExists == false)
        return false;

    // check if there is an empty drive to mount a tape
    for (std::shared_ptr<LTFSDMDrive> drive : inventory->getDrives()) {
        if (drive->isBusy() == true)
            continue;
        found = false;
        // check if there is a cartridge mounted in that drive:
        for (std::shared_ptr<LTFSDMCartridge> card : inventory->getCartridges()) {
            if (drive->get_le()->get_slot() == card->get_le()->get_slot()
                    && card->getState() == LTFSDMCartridge::TAPE_MOUNTED) {
                found = true;
                break;
            }
        }
        if (found == false) {
            std::shared_ptr<LTFSDMCartridge> cart;
            for (std::string cartname : Server::conf.getPool(pool)) {
                if ((cart = inventory->getCartridge(cartname)) == nullptr) {
                    MSG(LTFSDMX0034E, cartname);
                    Server::conf.poolRemove(pool, cartname);
                }
                if (cart->getState() == LTFSDMCartridge::TAPE_UNMOUNTED
                        && 1024 * 1024 * cart->get_le()->get_remaining_cap()
                                >= minFileSize) {
                    Scheduler::moveTape(drive->get_le()->GetObjectID(),
                            cartname,
                            Mount::MOUNT);
                    return false;
                }
            }

        }
    }

    /** @todo: check if the following needs to be moved before the
               for loop that is checking for a tape to mount
     */
    for (std::shared_ptr<LTFSDMDrive> drive : inventory->getDrives())
        if (drive->getUnmountReqNum() == reqNum)
            return false;

    // check if there is a tape to unmount
    for (std::shared_ptr<LTFSDMDrive> drive : inventory->getDrives()) {
        if (drive->isBusy() == true)
            continue;
        for (std::shared_ptr<LTFSDMCartridge> cart : inventory->getCartridges()) {
            if ((drive->get_le()->get_slot() == cart->get_le()->get_slot())
                    && (cart->getState() == LTFSDMCartridge::TAPE_MOUNTED)) {
                Scheduler::moveTape(drive->get_le()->GetObjectID(),
                        cart->get_le()->GetObjectID(),
                        Mount::UNMOUNT);
                return false;
            }
        }
    }

    return false;
}

bool Scheduler::tapeResAvail()

{
    bool found;

    assert(tapeId.compare("") != 0);

    if (inventory->getCartridge(tapeId)->getState()
            == LTFSDMCartridge::TAPE_MOVING)
        return false;

    /*
     * a tape is also changing its state to "TAPE_INUSE" when an
     * un/mount request is added to the REQUEST_QUEUE
     */
    if (inventory->getCartridge(tapeId)->getState()
            == LTFSDMCartridge::TAPE_INUSE) {
        found = false;
        for (std::shared_ptr<LTFSDMDrive> drive : inventory->getDrives()) {
            if (drive->get_le()->get_slot()
                    == inventory->getCartridge(tapeId)->get_le()->get_slot()) {
                TRACE(Trace::always, op, drive->getToUnblock(),
                        drive->get_le()->GetObjectID());
                if (op < drive->getToUnblock())
                    drive->setToUnblock(op);
                found = true;
                break;
            }
        }
        return false;
    } else if (inventory->getCartridge(tapeId)->getState()
            == LTFSDMCartridge::TAPE_MOUNTED) {
        found = false;
        for (std::shared_ptr<LTFSDMDrive> drive : inventory->getDrives()) {
            if (drive->get_le()->get_slot()
                    == inventory->getCartridge(tapeId)->get_le()->get_slot()) {
                assert(drive->isBusy() == false);
                TRACE(Trace::always, drive->get_le()->GetObjectID());
                driveId = drive->get_le()->GetObjectID();
                Scheduler::makeUse(driveId, tapeId);
                found = true;
                break;
            }
        }
        assert(found == true);
        return true;
    }

    // looking for a free drive
    for (std::shared_ptr<LTFSDMDrive> drive : inventory->getDrives()) {
        if (drive->isBusy() == true)
            continue;
        found = false;
        for (std::shared_ptr<LTFSDMCartridge> card : inventory->getCartridges()) {
            if ((drive->get_le()->get_slot() == card->get_le()->get_slot())
                    && (card->getState() == LTFSDMCartridge::TAPE_MOUNTED)) {
                found = true;
                break;
            }
        }
        if (found == false) {
            if (inventory->getCartridge(tapeId)->getState()
                    == LTFSDMCartridge::TAPE_UNMOUNTED) {
                Scheduler::moveTape(drive->get_le()->GetObjectID(),
                        tapeId, Mount::MOUNT);
                return false;
            }
        }
    }

    // looking for a tape to unmount
    for (std::shared_ptr<LTFSDMDrive> drive : inventory->getDrives()) {
        if (drive->isBusy() == true)
            continue;
        for (std::shared_ptr<LTFSDMCartridge> cart : inventory->getCartridges()) {
            if ((drive->get_le()->get_slot() == cart->get_le()->get_slot())
                    && (cart->getState() == LTFSDMCartridge::TAPE_MOUNTED)) {
                Scheduler::moveTape(drive->get_le()->GetObjectID(),
                        cart->get_le()->GetObjectID(),
                        Mount::UNMOUNT);
                inventory->getCartridge(tapeId)->unsetRequested();
                return false;
            }
        }
    }

    if (inventory->getCartridge(tapeId)->isRequested())
        return false;

    // suspend an operation
    for (std::shared_ptr<LTFSDMDrive> drive : inventory->getDrives()) {
        if (op < drive->getToUnblock()) {
            TRACE(Trace::always, op, drive->getToUnblock(),
                    drive->get_le()->GetObjectID());
            drive->setToUnblock(op);
            inventory->getCartridge(tapeId)->setRequested();
            break;
        }
    }

    return false;
}

bool Scheduler::resAvail(unsigned long minFileSize)

{
    if (op == DataBase::MOUNT || op == DataBase::UNMOUNT)
        return true;
    if (op == DataBase::MIGRATION && tapeId.compare("") == 0)
        return poolResAvail(minFileSize);
    else
        return tapeResAvail();
}

unsigned long Scheduler::smallestMigJob(int reqNum, int replNum)

{
    unsigned long min;

    SQLStatement stmt = SQLStatement(Scheduler::SMALLEST_MIG_JOB) << reqNum
            << FsObj::RESIDENT << replNum;
    stmt.prepare();
    stmt.step(&min);
    stmt.finalize();

    return min;
}

void Scheduler::run(long key)

{
    TRACE(Trace::normal, __PRETTY_FUNCTION__);

    SQLStatement selstmt;
    SQLStatement updstmt;
    std::stringstream ssql;
    std::unique_lock<std::mutex> lock(mtx);
    unsigned long minFileSize;

    while (true) {
        cond.wait(lock);
        if (Server::terminate == true) {
            TRACE(Trace::always, (bool) Server::terminate);
            lock.unlock();
            break;
        }

        selstmt(Scheduler::SELECT_REQUEST) << DataBase::REQ_NEW;

        selstmt.prepare();
        while (selstmt.step(&op, &reqNum, &tgtState, &numRepl, &replNum, &pool,
                &tapeId, &driveId)) {
            std::lock_guard<std::recursive_mutex> lock(LTFSDMInventory::mtx);

            TRACE(Trace::always, op, reqNum, replNum, tapeId, driveId);

            if (op == DataBase::MIGRATION)
                minFileSize = smallestMigJob(reqNum, replNum);
            else
                minFileSize = 0;

            if (resAvail(minFileSize) == false)
                continue;

            TRACE(Trace::always, reqNum, tgtState, numRepl, replNum, pool);

            std::stringstream thrdinfo;

            switch (op) {
                case DataBase::MOUNT:
                case DataBase::UNMOUNT:
                    updstmt(Scheduler::UPDATE_MNT_REQUEST)
                            << DataBase::REQ_INPROGRESS << reqNum;
                    updstmt.doall();

                    if ( op == DataBase::MOUNT )
                        thrdinfo << "MNT(" << tapeId << ")";
                    else
                        thrdinfo << "UMN(" << tapeId << ")";

                    subs.enqueue(thrdinfo.str(), &Mount::execRequest,
                            Mount(driveId, tapeId, reqNum, op == DataBase::MOUNT ? Mount::MOUNT : Mount::UNMOUNT));
                    break;
                case DataBase::MIGRATION:
                    updstmt(Scheduler::UPDATE_MIG_REQUEST)
                            << DataBase::REQ_INPROGRESS << tapeId << reqNum
                            << replNum << pool;
                    updstmt.doall();

                    thrdinfo << "M(" << reqNum << "," << replNum << "," << pool
                            << ")";

                    subs.enqueue(thrdinfo.str(), &Migration::execRequest,
                            Migration(getpid(), reqNum, { }, numRepl, tgtState),
                            replNum, driveId, pool, tapeId,
                            true /* needsTape */);
                    break;
                case DataBase::SELRECALL:
                    updstmt(Scheduler::UPDATE_REC_REQUEST)
                            << DataBase::REQ_INPROGRESS << reqNum << tapeId;
                    updstmt.doall();

                    thrdinfo << "SR(" << reqNum << ")";
                    subs.enqueue(thrdinfo.str(), &SelRecall::execRequest,
                            SelRecall(getpid(), reqNum, tgtState), driveId,
                            tapeId,
                            true /* needsTape */);
                    break;
                case DataBase::TRARECALL:
                    updstmt(Scheduler::UPDATE_REC_REQUEST)
                            << DataBase::REQ_INPROGRESS << reqNum << tapeId;
                    updstmt.doall();

                    thrdinfo << "TR(" << reqNum << ")";
                    subs.enqueue(thrdinfo.str(), &TransRecall::execRequest,
                            TransRecall(), reqNum, driveId, tapeId);
                    break;
                default:
                    TRACE(Trace::error, op);
            }
        }
        selstmt.finalize();
    }
    MSG(LTFSDMS0081I);
    subs.waitAllRemaining();
    MSG(LTFSDMS0082I);
}
