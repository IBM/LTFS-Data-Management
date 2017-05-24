#include "ServerIncludes.h"

OpenLTFSPool::OpenLTFSPool(std::string _poolName)

{
	if ( _poolName.compare("") == 0 ) {
		MSG(LTFSDMX0020E);
		throw(Error::LTFSDM_GENERAL_ERROR);
	}

	poolName = _poolName;
}

void OpenLTFSPool::add(std::shared_ptr<OpenLTFSCartridge> cartridge)

{
	if ( cartridge->getPool().compare("") != 0 ) {
		MSG(LTFSDMX0021E, cartridge->GetObjectID());
		throw(Error::LTFSDM_TAPE_EXISTS_IN_POOL);
	}

	cartridge->setPool(poolName);
	cartridges.push_back(cartridge);
}

void OpenLTFSPool::remove(std::shared_ptr<OpenLTFSCartridge> cartridge)

{
	for ( std::shared_ptr<OpenLTFSCartridge> ctrg : cartridges ) {
		if ( ctrg == cartridge ) {
			cartridge->setPool("");
			cartridges.remove(cartridge);
			return;
		}
	}

	MSG(LTFSDMX0022E, cartridge->GetObjectID(), poolName);
	throw(Error::LTFSDM_TAPE_NOT_EXISTS_IN_POOL);
}

std::list<std::shared_ptr<OpenLTFSCartridge>> OpenLTFSPool::getCartridges()

{
	return cartridges;
}
