#include "ServerIncludes.h"

OpenLTFSDrive::OpenLTFSDrive(ltfsadmin::Drive drive) : ltfsadmin::Drive(drive), busy(false), umountReqNum(Const::UNSET)

{
	wqp = new WorkQueue<std::string, long, long, Migration::mig_info_t>(&Migration::preMigrate, 16, "pmig-wq");
}

OpenLTFSDrive::~OpenLTFSDrive() {}

void OpenLTFSDrive::update(std::shared_ptr<LTFSAdminSession> sess)

{
	Drive *drive = dynamic_cast<Drive*>(this);
	*drive = *(LEControl::InventoryDrive(GetObjectID(), sess));
}
