#include "ServerIncludes.h"

void OpenLTFSCartridge::update(std::shared_ptr<LTFSAdminSession> sess)

{
	std::lock_guard < std::recursive_mutex > lock(OpenLTFSInventory::mtx);

	Cartridge *cartridge = dynamic_cast<Cartridge*>(this);
	*cartridge = *(LEControl::InventoryCartridge(GetObjectID(), sess));
}

void OpenLTFSCartridge::setInProgress(unsigned long size)

{
	std::lock_guard < std::recursive_mutex > lock(OpenLTFSInventory::mtx);

	inProgress = size;
}

unsigned long OpenLTFSCartridge::getInProgress()

{
	std::lock_guard < std::recursive_mutex > lock(OpenLTFSInventory::mtx);

	return inProgress;
}

void OpenLTFSCartridge::setPool(std::string _pool)

{
	std::lock_guard < std::recursive_mutex > lock(OpenLTFSInventory::mtx);

	pool = _pool;
}

std::string OpenLTFSCartridge::getPool()

{
	std::lock_guard < std::recursive_mutex > lock(OpenLTFSInventory::mtx);

	return pool;
}
void OpenLTFSCartridge::setState(state_t _state)

{
	std::lock_guard < std::recursive_mutex > lock(OpenLTFSInventory::mtx);

	state = _state;
}

OpenLTFSCartridge::state_t OpenLTFSCartridge::getState()

{
	std::lock_guard < std::recursive_mutex > lock(OpenLTFSInventory::mtx);

	return state;
}

bool OpenLTFSCartridge::isRequested()

{
	std::lock_guard < std::recursive_mutex > lock(OpenLTFSInventory::mtx);

	return requested;
}

void OpenLTFSCartridge::setRequested()

{
	std::lock_guard < std::recursive_mutex > lock(OpenLTFSInventory::mtx);

	requested = true;
}

void OpenLTFSCartridge::unsetRequested()

{
	std::lock_guard < std::recursive_mutex > lock(OpenLTFSInventory::mtx);

	requested = false;
}
