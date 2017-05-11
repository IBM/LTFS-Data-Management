#include "ServerIncludes.h"

OpenLTFSPool::OpenLTFSPool(std::string _poolName)

{
	if ( _poolName.compare("") == 0 ) {
		MSG(LTFSDMS0061E);
		throw(Error::LTFSDM_GENERAL_ERROR);
	}

	poolName = _poolName;
}

void OpenLTFSPool::add(std::shared_ptr<OpenLTFSCartridge> cartridge)

{
	for ( std::shared_ptr<OpenLTFSCartridge> ctrg : cartridges ) {
		if ( ctrg == cartridge ) {
			MSG(LTFSDMS0062E, ctrg->GetObjectID());
			throw(Error::LTFSDM_TAPE_EXISTS_IN_POOL);
		}
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

	MSG(LTFSDMS0063E, cartridge->GetObjectID(), poolName);
	throw(Error::LTFSDM_TAPE_NOT_EXISTS_IN_POOL);
}

std::list<OpenLTFSCartridge> OpenLTFSPool::getCartridges()

{
	std::list<OpenLTFSCartridge> ctds;

	for ( std::shared_ptr<OpenLTFSCartridge> i : cartridges )
		ctds.push_back(*i);

	return ctds;
}
