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

void TapeHandler::addRequest()

{
    SQLStatement stmt;
    long reqNumber = ++globalReqNumber;

    std::lock_guard<std::recursive_mutex> lock(LTFSDMInventory::mtx);

    TRACE(Trace::always, op, tapeId, poolName);

    stmt(TapeHandler::ADD_REQUEST)
            << (op == TapeHandler::FORMAT ? DataBase::FORMAT : DataBase::CHECK)
            << reqNumber << Const::UNSET <<  tapeId << poolName << time(NULL) << DataBase::REQ_NEW;

    TRACE(Trace::normal, stmt.str());

    stmt.doall();
}

void TapeHandler::execRequest()

{
    std::shared_ptr<LTFSDMCartridge> cart;
    SQLStatement stmt;

    TRACE(Trace::always, op, driveId, tapeId, poolName);

    try {
        if ((cart = inventory->getCartridge(tapeId)) == nullptr) {
            MSG(LTFSDMX0034E, tapeId);
            THROW(Error::GENERAL_ERROR, tapeId);
        }

        try {
            cart->get_le()->Remove(false, false, true);
        } catch ( AdminLibException& e ) {
            MSG(LTFSDMS0114W, tapeId, e.what());
        }

        if ( op == TapeHandler::FORMAT) {
            try {
                cart->get_le()->Format(driveId, 0, true);
            } catch ( AdminLibException& e ) {
                MSG(LTFSDMS0115E, tapeId, e.what());
                THROW(Error::GENERAL_ERROR);
            }
        }
        else {
            try {
                cart->get_le()->Check(driveId);
            } catch ( AdminLibException& e ) {
                MSG(LTFSDMS0116E, tapeId, e.what());
                THROW(Error::GENERAL_ERROR);
            }
        }

        try {
            inventory->poolAdd(poolName, tapeId);
        } catch (const LTFSDMException& e) {
            cart->result = e.getError();
            MSG(LTFSDMS0117E, tapeId, poolName, e.what());
        } catch (const std::exception& e) {
            cart->result = Error::GENERAL_ERROR;
            MSG(LTFSDMS0117E, tapeId, poolName, e.what());
        }

        stmt(TapeHandler::DELETE_REQUEST) << reqNum;

        TRACE(Trace::normal, stmt.str());

        stmt.doall();
    } catch (const std::exception& e) {
        MSG(LTFSDMS0109E, tapeId, poolName);
    }

    TRACE(Trace::always, driveId, tapeId, poolName);

    {
        std::lock_guard<std::recursive_mutex> llock(LTFSDMInventory::mtx);
        if (inventory->getCartridge(tapeId)->getState()
                == LTFSDMCartridge::TAPE_INUSE)
            inventory->getCartridge(tapeId)->setState(
                    LTFSDMCartridge::TAPE_MOUNTED);

        inventory->getDrive(driveId)->setFree();
        inventory->getDrive(driveId)->clearToUnblock();
    }

    try {
        inventory->inventorize();
    } catch (const std::exception& e) {
        MSG(LTFSDMS0101E, e.what());
    }

    std::unique_lock<std::mutex> lock(cart->mtx);
    cart->cond.notify_one();

    Scheduler::invoke();
}
