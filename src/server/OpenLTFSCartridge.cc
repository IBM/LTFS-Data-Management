#include "ServerIncludes.h"

void OpenLTFSCartridge::update(std::shared_ptr<LTFSAdminSession> sess)

{
	Cartridge *cartridge = dynamic_cast<Cartridge*>(this);
	*cartridge = *(LEControl::InventoryCartridge(GetObjectID(), sess));
}
