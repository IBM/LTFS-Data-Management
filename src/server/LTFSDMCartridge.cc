#include "ServerIncludes.h"

LTFSDMCartridge::LTFSDMCartridge(boost::shared_ptr<Cartridge> c) :
            cart(c), inProgress(0), pool(""), requested(false),
            state(LTFSDMCartridge::TAPE_UNKNOWN)
{
}

void LTFSDMCartridge::update()

{
    std::lock_guard<std::recursive_mutex> lock(LTFSDMInventory::mtx);

    cart = inventory->lookupCartridge(cart->GetObjectID());
}

void LTFSDMCartridge::setInProgress(unsigned long size)

{
    std::lock_guard<std::recursive_mutex> lock(LTFSDMInventory::mtx);

    inProgress = size;
}

unsigned long LTFSDMCartridge::getInProgress()

{
    std::lock_guard<std::recursive_mutex> lock(LTFSDMInventory::mtx);

    return inProgress;
}

void LTFSDMCartridge::setPool(std::string _pool)

{
    std::lock_guard<std::recursive_mutex> lock(LTFSDMInventory::mtx);

    pool = _pool;
}

std::string LTFSDMCartridge::getPool()

{
    std::lock_guard<std::recursive_mutex> lock(LTFSDMInventory::mtx);

    return pool;
}
void LTFSDMCartridge::setState(state_t _state)

{
    std::lock_guard<std::recursive_mutex> lock(LTFSDMInventory::mtx);

    state = _state;
}

LTFSDMCartridge::state_t LTFSDMCartridge::getState()

{
    std::lock_guard<std::recursive_mutex> lock(LTFSDMInventory::mtx);

    return state;
}

bool LTFSDMCartridge::isRequested()

{
    std::lock_guard<std::recursive_mutex> lock(LTFSDMInventory::mtx);

    return requested;
}

void LTFSDMCartridge::setRequested()

{
    std::lock_guard<std::recursive_mutex> lock(LTFSDMInventory::mtx);

    requested = true;
}

void LTFSDMCartridge::unsetRequested()

{
    std::lock_guard<std::recursive_mutex> lock(LTFSDMInventory::mtx);

    requested = false;
}
