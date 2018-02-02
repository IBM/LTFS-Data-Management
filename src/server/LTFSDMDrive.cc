#include "ServerIncludes.h"

LTFSDMDrive::LTFSDMDrive(boost::shared_ptr<Drive> d) :
        drive(d), busy(false), umountReqNum(Const::UNSET),
        toUnBlock(DataBase::NOOP), mtx(nullptr), wqp(nullptr) {}

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
}

void LTFSDMDrive::setFree()

{
    std::lock_guard<std::recursive_mutex> lock(LTFSDMInventory::mtx);

    busy = false;
}

void LTFSDMDrive::setUnmountReqNum(int reqnum)

{
    std::lock_guard<std::recursive_mutex> lock(LTFSDMInventory::mtx);

    umountReqNum = reqnum;
}

int LTFSDMDrive::getUnmountReqNum()

{
    std::lock_guard<std::recursive_mutex> lock(LTFSDMInventory::mtx);

    return umountReqNum;
}

void LTFSDMDrive::unsetUnmountReqNum()

{
    std::lock_guard<std::recursive_mutex> lock(LTFSDMInventory::mtx);

    umountReqNum = Const::UNSET;
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
