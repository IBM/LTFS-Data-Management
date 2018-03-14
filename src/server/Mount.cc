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


long Mount::addRequest()

{
    SQLStatement stmt;
    long reqNumber = ++globalReqNumber;


    stmt(Mount::ADD_REQUEST)
            << (op == Mount::MOUNT ? DataBase::MOUNT : DataBase::UNMOUNT)
            << reqNumber << tapeId << time(NULL) << DataBase::REQ_NEW;

    TRACE(Trace::normal, stmt.str());

    stmt.doall();

    return reqNumber;
}

void Mount::execRequest()

{
    std::shared_ptr<LTFSDMCartridge> cart;
    SQLStatement stmt;

    try {
        if ((cart = inventory->getCartridge(tapeId)) == nullptr) {
            MSG(LTFSDMX0034E, tapeId);
            THROW(Error::GENERAL_ERROR, tapeId);
        }

        cart->setState(LTFSDMCartridge::TAPE_MOVING);

        if ( op == Mount::MOUNT) {
            inventory->mount(driveId, tapeId);
        }
        else {
            inventory->unmount(driveId, tapeId);
        }

        stmt(Mount::DELETE_REQUEST) << reqNum;

        TRACE(Trace::normal, stmt.str());

        stmt.doall();
    } catch (const std::exception& e) {
        MSG(LTFSDMS0104E, tapeId);
    }

    TRACE(Trace::always, driveId, tapeId);

    std::unique_lock<std::mutex> lock(Scheduler::mtx);
    Scheduler::cond.notify_one();
}

