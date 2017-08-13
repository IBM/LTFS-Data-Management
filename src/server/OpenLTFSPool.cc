#include "ServerIncludes.h"

OpenLTFSPool::OpenLTFSPool(std::string _poolName)

{
    std::lock_guard<std::recursive_mutex> lock(OpenLTFSInventory::mtx);

    if (_poolName.compare("") == 0) {
        MSG(LTFSDMX0020E);
        THROW(Const::UNSET, _poolName);
    }

    poolName = _poolName;
}

std::string OpenLTFSPool::getPoolName()

{
    std::lock_guard<std::recursive_mutex> lock(OpenLTFSInventory::mtx);

    return poolName;
}

void OpenLTFSPool::add(std::shared_ptr<OpenLTFSCartridge> cartridge)

{
    std::lock_guard<std::recursive_mutex> lock(OpenLTFSInventory::mtx);

    if (cartridge->getPool().compare("") != 0) {
        MSG(LTFSDMX0021E, cartridge->GetObjectID());
        THROW(Error::LTFSDM_TAPE_EXISTS_IN_POOL);
    }

    cartridge->setPool(poolName);
    cartridges.push_back(cartridge);
}

void OpenLTFSPool::remove(std::shared_ptr<OpenLTFSCartridge> cartridge)

{
    std::lock_guard<std::recursive_mutex> lock(OpenLTFSInventory::mtx);

    for (std::shared_ptr<OpenLTFSCartridge> ctrg : cartridges) {
        if (ctrg == cartridge) {
            cartridge->setPool("");
            cartridges.remove(cartridge);
            return;
        }
    }

    MSG(LTFSDMX0022E, cartridge->GetObjectID(), poolName);
    THROW(Error::LTFSDM_TAPE_NOT_EXISTS_IN_POOL);
}

std::list<std::shared_ptr<OpenLTFSCartridge>> OpenLTFSPool::getCartridges()

{
    std::lock_guard<std::recursive_mutex> lock(OpenLTFSInventory::mtx);

    return cartridges;
}
