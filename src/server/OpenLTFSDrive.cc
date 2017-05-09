#include "ServerIncludes.h"

void OpenLTFSDrive::update(std::shared_ptr<LTFSAdminSession> sess)

{
	Drive *drive = dynamic_cast<Drive*>(this);
	*drive = *(LEControl::InventoryDrive(GetObjectID(), sess));
}
