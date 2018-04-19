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

LTFSDMDrive::LTFSDMDrive(boost::shared_ptr<Drive> d) :
        drive(d), busy(false), umountReqNum(Const::UNSET), umountReqPool(""),
        toUnBlock(DataBase::NOOP), mtx(nullptr), wqp(nullptr)
{
}

LTFSDMDrive::~LTFSDMDrive()
{
    delete (mtx);
}

void LTFSDMDrive::update()

{
    std::lock_guard<std::recursive_mutex> lock(LTFSDMInventory::mtx);

    drive = inventory->lookupDrive(drive->GetObjectID());
}

bool LTFSDMDrive::isBusy()

{
    std::lock_guard<std::recursive_mutex> lock(LTFSDMInventory::mtx);

    return busy;
}

void LTFSDMDrive::setBusy()

{
    std::lock_guard<std::recursive_mutex> lock(LTFSDMInventory::mtx);

    busy = true;

    TRACE(Trace::always, this->get_le()->GetObjectID(), busy);
}

void LTFSDMDrive::setFree()

{
    std::lock_guard<std::recursive_mutex> lock(LTFSDMInventory::mtx);

    busy = false;

    TRACE(Trace::always, this->get_le()->GetObjectID(), busy);
}

void LTFSDMDrive::setMoveReq(int reqnum, std::string pool)

{
    std::lock_guard<std::recursive_mutex> lock(LTFSDMInventory::mtx);

    umountReqNum = reqnum;
    umountReqPool = pool;
}

int LTFSDMDrive::getMoveReqNum()

{
    std::lock_guard<std::recursive_mutex> lock(LTFSDMInventory::mtx);

    return umountReqNum;
}

std::string LTFSDMDrive::getMoveReqPool()

{
    std::lock_guard<std::recursive_mutex> lock(LTFSDMInventory::mtx);

    return umountReqPool;
}

void LTFSDMDrive::unsetMoveReq()

{
    std::lock_guard<std::recursive_mutex> lock(LTFSDMInventory::mtx);

    umountReqNum = Const::UNSET;
    umountReqPool = "";
}

void LTFSDMDrive::setToUnblock(DataBase::operation op)

{
    std::lock_guard<std::recursive_mutex> lock(LTFSDMInventory::mtx);

    if (op < toUnBlock)
        toUnBlock = op;
}

DataBase::operation LTFSDMDrive::getToUnblock()

{
    std::lock_guard<std::recursive_mutex> lock(LTFSDMInventory::mtx);

    return toUnBlock;
}

void LTFSDMDrive::clearToUnblock()

{
    std::lock_guard<std::recursive_mutex> lock(LTFSDMInventory::mtx);

    toUnBlock = DataBase::NOOP;
}
