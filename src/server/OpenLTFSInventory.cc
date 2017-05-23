#include "ServerIncludes.h"

OpenLTFSInventory *inventory = NULL;

void OpenLTFSInventory::writePools()

{
	{
		bool first;
		std::ofstream conffiletmp(Const::TMP_CONFIG_FILE, conffiletmp.trunc);

		for ( std::shared_ptr<OpenLTFSPool>  pool : pools ) {
			first = true;
			conffiletmp << pool->getPoolName() << std::endl;
			for ( OpenLTFSCartridge cartridge : pool->getCartridges() ) {
				conffiletmp << (first ? "" : " ") << cartridge.GetObjectID();
				first = false;
			}
			conffiletmp << std::endl;
		}
	}

	if ( rename(Const::TMP_CONFIG_FILE.c_str(), Const::CONFIG_FILE.c_str()) == -1 )
		MSG(LTFSDMS0062E);
}

OpenLTFSInventory::OpenLTFSInventory()

{
	std::lock_guard<std::mutex> llck(mtx);
	std::list<std::shared_ptr<Drive> > drvs;
	std::list<std::shared_ptr<Cartridge>> crts;
	std::ifstream conffile(Const::CONFIG_FILE);
	std::string line;
	int rc;

	sess = LEControl::Connect("127.0.0.1", 7600);

	rc = LEControl::InventoryDrive(drvs, sess);

	if ( rc == -1 || drvs.size() == 0 ) {
		MSG(LTFSDMS0051E);
		LEControl::Disconnect(sess);
		throw(Error::LTFSDM_GENERAL_ERROR);
	}

	for (std::shared_ptr<Drive> i : drvs) {
		TRACE(Trace::always, i->GetObjectID());
		MSG(LTFSDMS0052I, i->GetObjectID());
		drives.push_back(std::make_shared<OpenLTFSDrive>(OpenLTFSDrive(*i)));
	}

	rc = LEControl::InventoryCartridge(crts, sess);

	if ( rc == -1 || crts.size() == 0 ) {
		MSG(LTFSDMS0053E);
		LEControl::Disconnect(sess);
		throw(Error::LTFSDM_GENERAL_ERROR);
	}

	for(std::shared_ptr<Cartridge> c : crts) {
		if ( c->get_status().compare("Not Supported") == 0 )
			continue;
		TRACE(Trace::always, c->GetObjectID());
		MSG(LTFSDMS0054I, c->GetObjectID());
		cartridges.push_back(std::make_shared<OpenLTFSCartridge>(OpenLTFSCartridge(*c)));
	}

	while (std::getline(conffile, line))
	{
		std::string poolName = line;
		std::list<std::string> tapeids;
		std::string::size_type pos = 0;

		if ( ! std::getline(conffile, line) )
			return;

		while ((pos = line.find(" ")) != std::string::npos) {
			tapeids.push_back(line.substr(0, pos));
			line.erase(0, pos + 1);
		}

		tapeids.push_back(line);

		try {
			poolCreate(poolName);
			for ( std::string tapeid : tapeids )
				poolAdd(poolName, tapeid);
		}
		catch(int error) {
			MSG(LTFSDMS0061E, poolName);
			throw(Error::LTFSDM_GENERAL_ERROR);
		}
	}

	writePools();

	for ( std::shared_ptr<OpenLTFSCartridge> c : cartridges ) {
		c->setState(OpenLTFSCartridge::UNMOUNTED);
		for ( std::shared_ptr<OpenLTFSDrive> d : drives ) {
			if ( c->get_slot() == d->get_slot() ) {
				c->setState(OpenLTFSCartridge::MOUNTED);
				break;
			}
		}
	}
}

void OpenLTFSInventory::reinventorize()

{
}


std::list<OpenLTFSDrive> OpenLTFSInventory::getDrives()

{
	std::list<OpenLTFSDrive> driveids;

	for ( std::shared_ptr<OpenLTFSDrive> drive : drives )
		driveids.push_back(*drive);

	return driveids;
}

std::shared_ptr<OpenLTFSDrive> OpenLTFSInventory::getDrive(std::string driveid)

{
	for ( std::shared_ptr<OpenLTFSDrive> drive : drives )
		if ( drive->GetObjectID().compare(driveid) == 0 )
			return drive;

	return nullptr;
}

std::list<OpenLTFSCartridge> OpenLTFSInventory::getCartridges()

{
	std::list<OpenLTFSCartridge> crtds;

	for ( std::shared_ptr<OpenLTFSCartridge> cartridge : cartridges )
		crtds.push_back(*cartridge);

	return crtds;
}

std::shared_ptr<OpenLTFSCartridge> OpenLTFSInventory::getCartridge(std::string cartridgeid)

{
	for ( std::shared_ptr<OpenLTFSCartridge> cartridge : cartridges )
		if ( cartridge->GetObjectID().compare(cartridgeid) == 0 )
			return cartridge;

	return nullptr;
}


std::list<OpenLTFSPool> OpenLTFSInventory::getPools()

{
	std::list<OpenLTFSPool> pls;

	for ( std::shared_ptr<OpenLTFSPool> pool: pools )
		pls.push_back(*pool);

	return pls;
}

std::shared_ptr<OpenLTFSPool> OpenLTFSInventory::getPool(std::string poolname)

{
	for ( std::shared_ptr<OpenLTFSPool> pool : pools )
		if ( pool->getPoolName().compare(poolname) == 0 )
			return pool;

	return nullptr;
}


void OpenLTFSInventory::poolCreate(std::string poolname)

{
	for ( std::shared_ptr<OpenLTFSPool> pool : pools ) {
		if ( pool->getPoolName().compare(poolname) == 0 ) {
			MSG(LTFSDMX0023E, poolname);
			throw(Error::LTFSDM_POOL_EXISTS);
		}
	}

	pools.push_back(std::make_shared<OpenLTFSPool>(OpenLTFSPool(poolname)));
	writePools();
}

void OpenLTFSInventory::poolDelete(std::string poolname)

{
	for ( std::shared_ptr<OpenLTFSPool> pool : pools ) {
		if ( pool->getPoolName().compare(poolname) == 0 ) {
			if ( pool->getCartridges().size() > 0 ) {
				MSG(LTFSDMX0024E, poolname);
				throw(Error::LTFSDM_POOL_NOT_EMPTY);
			}
			pools.remove(pool);
			writePools();
			return;
		}
	}

	MSG(LTFSDMX0025E, poolname);
	throw(Error::LTFSDM_POOL_NOT_EXISTS);
}

void OpenLTFSInventory::poolAdd(std::string poolname, std::string cartridgeid)

{
	std::shared_ptr<OpenLTFSCartridge> cartridge;

	for ( std::shared_ptr<OpenLTFSPool> pool : pools ) {
		if ( pool->getPoolName().compare(poolname) == 0 ) {
			cartridge = getCartridge(cartridgeid);
			if ( cartridge == nullptr )
				throw(Error::LTFSDM_TAPE_NOT_EXISTS);
			pool->add(cartridge);
			writePools();
			return;
		}
	}

	MSG(LTFSDMX0025E, poolname);
	throw(Error::LTFSDM_POOL_NOT_EXISTS);
}

void OpenLTFSInventory::poolRemove(std::string poolname, std::string cartridgeid)

{
	std::shared_ptr<OpenLTFSCartridge> cartridge;

	for ( std::shared_ptr<OpenLTFSPool> pool : pools ) {
		if ( pool->getPoolName().compare(poolname) == 0 ) {
			cartridge = getCartridge(cartridgeid);
			if ( cartridge == nullptr )
				throw(Error::LTFSDM_TAPE_NOT_EXISTS);
			pool->remove(cartridge);
			writePools();
			return;
		}
	}

	MSG(LTFSDMX0024E, poolname);
	throw(Error::LTFSDM_POOL_NOT_EXISTS);
}


void OpenLTFSInventory::mount(std::string driveid, std::string cartridgeid)

{
	std::shared_ptr<OpenLTFSCartridge> ctg = nullptr;
	std::shared_ptr<OpenLTFSDrive> drv = nullptr;

	{
		std::lock_guard<std::mutex> llck(mtx);
		for ( std::shared_ptr<OpenLTFSCartridge> cartridge : cartridges )
			if ( cartridge->GetObjectID().compare(cartridgeid) == 0 )
				ctg = cartridge;

		if ( ctg == nullptr || ctg->getState() != OpenLTFSCartridge::UNMOUNTED )
			throw(Error::LTFSDM_GENERAL_ERROR);

		for ( std::shared_ptr<OpenLTFSDrive> drive : drives )
			if ( drive->GetObjectID().compare(driveid) == 0 )
				drv = drive;

		if ( drv == nullptr || drv->isBusy() == true )
			throw(Error::LTFSDM_GENERAL_ERROR);

		ctg->setState(OpenLTFSCartridge::MOVING);
		drv->setBusy();
	}

	ctg->Mount(driveid);

	{
		std::lock_guard<std::mutex> llck(mtx);

		ctg->update(sess);
		ctg->setState(OpenLTFSCartridge::MOUNTED);
		drv->setFree();
	}
}

void OpenLTFSInventory::unmount(std::string cartridgeid)

{
	std::shared_ptr<OpenLTFSCartridge> ctg = nullptr;
	std::shared_ptr<OpenLTFSDrive> drv = nullptr;
	int slot = Const::UNSET;

	{
		std::lock_guard<std::mutex> llck(mtx);
		for ( std::shared_ptr<OpenLTFSCartridge> cartridge : cartridges )
			if ( cartridge->GetObjectID().compare(cartridgeid) == 0 )
				ctg = cartridge;

		if ( ctg == nullptr || ctg->getState() != OpenLTFSCartridge::MOUNTED )
			throw(Error::LTFSDM_GENERAL_ERROR);

		for ( std::shared_ptr<OpenLTFSDrive> drive : drives ) {
			if ( drive->get_slot() == ctg->get_slot() ) {
				drv = drive;
				slot = ctg->get_slot();
			}
		}

		if ( drv->isBusy() == true  || slot == Const::UNSET )
			throw(Error::LTFSDM_GENERAL_ERROR);

		ctg->setState(OpenLTFSCartridge::MOVING);
		drv->setBusy();
	}

	ctg->Unmount();

	{
		std::lock_guard<std::mutex> llck(mtx);

		ctg->update(sess);
		ctg->setState(OpenLTFSCartridge::UNMOUNTED);
		drv->setFree();
	}
}

void OpenLTFSInventory::format(std::string cartridgeid)

{
}

void OpenLTFSInventory::check(std::string cartridgeid)

{
}

OpenLTFSInventory::~OpenLTFSInventory()

{
	LEControl::Disconnect(sess);
}
