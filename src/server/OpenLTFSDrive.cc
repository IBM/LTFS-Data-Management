#include "ServerIncludes.h"

void OpenLTFSDrive::update(std::shared_ptr<LTFSAdminSession> sess)

{
	std::lock_guard < std::recursive_mutex > lock(OpenLTFSInventory::mtx);

	Drive *drive = dynamic_cast<Drive*>(this);
	*drive = *(LEControl::InventoryDrive(GetObjectID(), sess));
}

bool OpenLTFSDrive::isBusy()

{
	std::lock_guard < std::recursive_mutex > lock(OpenLTFSInventory::mtx);

	return busy;
}

void OpenLTFSDrive::setBusy()

{
	std::lock_guard < std::recursive_mutex > lock(OpenLTFSInventory::mtx);

	busy = true;
}

void OpenLTFSDrive::setFree()

{
	std::lock_guard < std::recursive_mutex > lock(OpenLTFSInventory::mtx);

	busy = false;
}

void OpenLTFSDrive::setUnmountReqNum(int reqnum)

{
	std::lock_guard < std::recursive_mutex > lock(OpenLTFSInventory::mtx);

	umountReqNum = reqnum;
}

int OpenLTFSDrive::getUnmountReqNum()

{
	std::lock_guard < std::recursive_mutex > lock(OpenLTFSInventory::mtx);

	return umountReqNum;
}

void OpenLTFSDrive::unsetUnmountReqNum()

{
	std::lock_guard < std::recursive_mutex > lock(OpenLTFSInventory::mtx);

	umountReqNum = Const::UNSET;
}

void OpenLTFSDrive::setToUnblock(DataBase::operation op)

{
	std::lock_guard < std::recursive_mutex > lock(OpenLTFSInventory::mtx);

	if (op < toUnBlock)
		toUnBlock = op;
}

DataBase::operation OpenLTFSDrive::getToUnblock()

{
	std::lock_guard < std::recursive_mutex > lock(OpenLTFSInventory::mtx);

	return toUnBlock;
}

void OpenLTFSDrive::clearToUnblock()

{
	std::lock_guard < std::recursive_mutex > lock(OpenLTFSInventory::mtx);

	toUnBlock = DataBase::NOOP;
}
