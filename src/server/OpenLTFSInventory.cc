#include "ServerIncludes.h"

OpenLTFSInventory *inventory = NULL;
std::recursive_mutex OpenLTFSInventory::mtx;

void OpenLTFSInventory::writePools()

{
	std::lock_guard<std::recursive_mutex> lock(OpenLTFSInventory::mtx);

	{
		bool first;
		std::ofstream conffiletmp(Const::TMP_CONFIG_FILE, conffiletmp.trunc);

		for ( std::shared_ptr<OpenLTFSPool>  pool : pools ) {
			first = true;
			conffiletmp << pool->getPoolName() << std::endl;
			for ( std::shared_ptr<OpenLTFSCartridge> cartridge : pool->getCartridges() ) {
				conffiletmp << (first ? "" : " ") << cartridge->GetObjectID();
				first = false;
			}
			conffiletmp << std::endl;
		}
	}

	if ( rename(Const::TMP_CONFIG_FILE.c_str(), Const::CONFIG_FILE.c_str()) == -1 )
		MSG(LTFSDMS0062E);
}

void OpenLTFSInventory::inventorize()

{
	std::list<std::shared_ptr<Drive> > drvs;
	std::list<std::shared_ptr<Cartridge>> crts;
	std::ifstream conffile(Const::CONFIG_FILE);
	std::string line;
	int i = 0;
	int rc;

	std::lock_guard<std::recursive_mutex> lock(OpenLTFSInventory::mtx);

	for ( std::shared_ptr<OpenLTFSDrive> d : drives )
		if ( d->isBusy() == true )
			throw(EXCEPTION(Error::LTFSDM_DRIVE_BUSY));

	for ( std::shared_ptr<OpenLTFSDrive> d : drives )
		d->wqp->terminate();

	drives.clear();
	cartridges.clear();
	pools.clear();

	rc = LEControl::InventoryDrive(drvs, sess);

	if ( rc == -1 || drvs.size() == 0 ) {
		MSG(LTFSDMS0051E);
		LEControl::Disconnect(sess);
		throw(EXCEPTION(Const::UNSET));
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
		throw(EXCEPTION(Const::UNSET));
	}

	for(std::shared_ptr<Cartridge> c : crts) {
		if ( c->get_status().compare("Not Supported") == 0 )
			continue;
		TRACE(Trace::always, c->GetObjectID());
		MSG(LTFSDMS0054I, c->GetObjectID());
		cartridges.push_back(std::make_shared<OpenLTFSCartridge>(OpenLTFSCartridge(*c)));
	}

	cartridges.sort([] (const std::shared_ptr<Cartridge> c1, const std::shared_ptr<Cartridge> c2)
					{return (c1->GetObjectID().compare(c2->GetObjectID()) < 0);});

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
		catch(const std::exception& e) {
			TRACE(Trace::error, e.what());
			MSG(LTFSDMS0061E, poolName);
			throw(EXCEPTION(Const::UNSET));
		}
	}

	for ( std::shared_ptr<OpenLTFSCartridge> c : cartridges ) {
		c->setState(OpenLTFSCartridge::UNMOUNTED);
		for ( std::shared_ptr<OpenLTFSDrive> d : drives ) {
			if ( c->get_slot() == d->get_slot() ) {
				c->setState(OpenLTFSCartridge::MOUNTED);
				break;
			}
		}
	}

	for ( std::shared_ptr<OpenLTFSDrive> drive : drives ) {
		std::stringstream threadName;
		threadName << "pmig" << i++ << "-wq";
		drive->wqp = new ThreadPool<std::string, std::string, long, long, Migration::mig_info_t,
								   std::shared_ptr<std::list<unsigned long>>, std::shared_ptr<bool>>
			(&Migration::preMigrate, Const::NUM_PREMIG_THREADS, threadName.str());
	}
}

OpenLTFSInventory::OpenLTFSInventory()

{
	std::lock_guard<std::recursive_mutex> lock(OpenLTFSInventory::mtx);
	std::shared_ptr<ltfsadmin::LTFSNode> nodeInfo;
	struct stat statbuf;

	try {
		sess = LEControl::Connect("127.0.0.1", 7600);
	}
	catch ( const std::exception& e ) {
		TRACE(Trace::error, e.what());
		MSG(LTFSDMS0072E);
		throw(EXCEPTION(Const::UNSET));
	}

	if ( sess == nullptr ) {
		MSG(LTFSDMS0072E);
		throw(EXCEPTION(Const::UNSET));
	}

	try {
		nodeInfo = LEControl::InventoryNode(sess);
	}
	catch ( const std::exception& e ) {
		TRACE(Trace::error, e.what());
		MSG(LTFSDMS0072E);
		throw(EXCEPTION(Const::UNSET));
	}

	try {
		mountPoint = nodeInfo->get_mount_point();
	}
	catch ( const std::exception& e ) {
		TRACE(Trace::error, e.what());
		MSG(LTFSDMS0072E);
		throw(EXCEPTION(Const::UNSET));
	}

	TRACE(Trace::always, mountPoint);

	if ( stat(mountPoint.c_str(), &statbuf) == -1 ) {
		MSG(LTFSDMS0072E);
		throw(EXCEPTION(Const::UNSET, errno));
	}

	if ( nodeInfo == nullptr ) {
		MSG(LTFSDMS0072E);
		throw(EXCEPTION(Const::UNSET));
	}

	try {
		inventorize();
		writePools();
	}
	catch ( const std::exception& e ) {
		TRACE(Trace::error, e.what());
		LEControl::Disconnect(sess);
		throw(EXCEPTION(Const::UNSET));
	}
}

std::list<std::shared_ptr<OpenLTFSDrive>> OpenLTFSInventory::getDrives()

{
	std::lock_guard<std::recursive_mutex> lock(OpenLTFSInventory::mtx);

	return drives;
}

std::shared_ptr<OpenLTFSDrive> OpenLTFSInventory::getDrive(std::string driveid)

{
	std::lock_guard<std::recursive_mutex> lock(OpenLTFSInventory::mtx);

	for ( std::shared_ptr<OpenLTFSDrive> drive : drives )
		if ( drive->GetObjectID().compare(driveid) == 0 )
			return drive;

	return nullptr;
}

std::list<std::shared_ptr<OpenLTFSCartridge>> OpenLTFSInventory::getCartridges()

{
	std::lock_guard<std::recursive_mutex> lock(OpenLTFSInventory::mtx);

	return cartridges;
}

std::shared_ptr<OpenLTFSCartridge> OpenLTFSInventory::getCartridge(std::string cartridgeid)

{
	std::lock_guard<std::recursive_mutex> lock(OpenLTFSInventory::mtx);

	for ( std::shared_ptr<OpenLTFSCartridge> cartridge : cartridges )
		if ( cartridge->GetObjectID().compare(cartridgeid) == 0 )
			return cartridge;

	return nullptr;
}


std::list<std::shared_ptr<OpenLTFSPool>> OpenLTFSInventory::getPools()

{
	std::lock_guard<std::recursive_mutex> lock(OpenLTFSInventory::mtx);

	return pools;
}

std::shared_ptr<OpenLTFSPool> OpenLTFSInventory::getPool(std::string poolname)

{
	std::lock_guard<std::recursive_mutex> lock(OpenLTFSInventory::mtx);

	for ( std::shared_ptr<OpenLTFSPool> pool : pools )
		if ( pool->getPoolName().compare(poolname) == 0 )
			return pool;

	return nullptr;
}

void OpenLTFSInventory::update(std::shared_ptr<OpenLTFSDrive> drive)

{
	std::lock_guard<std::recursive_mutex> lock(OpenLTFSInventory::mtx);

	drive->update(sess);
}

void OpenLTFSInventory::update(std::shared_ptr<OpenLTFSCartridge> cartridge)

{
	std::lock_guard<std::recursive_mutex> lock(OpenLTFSInventory::mtx);

	cartridge->update(sess);
}

void OpenLTFSInventory::poolCreate(std::string poolname)

{
	std::lock_guard<std::recursive_mutex> lock(OpenLTFSInventory::mtx);

	for ( std::shared_ptr<OpenLTFSPool> pool : pools ) {
		if ( pool->getPoolName().compare(poolname) == 0 ) {
			MSG(LTFSDMX0023E, poolname);
			throw(EXCEPTION(Error::LTFSDM_POOL_EXISTS));
		}
	}

	pools.push_back(std::make_shared<OpenLTFSPool>(OpenLTFSPool(poolname)));
}

void OpenLTFSInventory::poolDelete(std::string poolname)

{
	std::lock_guard<std::recursive_mutex> lock(OpenLTFSInventory::mtx);

	for ( std::shared_ptr<OpenLTFSPool> pool : pools ) {
		if ( pool->getPoolName().compare(poolname) == 0 ) {
			if ( pool->getCartridges().size() > 0 ) {
				MSG(LTFSDMX0024E, poolname);
				throw(EXCEPTION(Error::LTFSDM_POOL_NOT_EMPTY));
			}
			pools.remove(pool);
			return;
		}
	}

	MSG(LTFSDMX0025E, poolname);
	throw(EXCEPTION(Error::LTFSDM_POOL_NOT_EXISTS));
}

void OpenLTFSInventory::poolAdd(std::string poolname, std::string cartridgeid)

{
	std::lock_guard<std::recursive_mutex> lock(OpenLTFSInventory::mtx);

	std::shared_ptr<OpenLTFSCartridge> cartridge;

	for ( std::shared_ptr<OpenLTFSPool> pool : pools ) {
		if ( pool->getPoolName().compare(poolname) == 0 ) {
			cartridge = getCartridge(cartridgeid);
			if ( cartridge == nullptr )
				throw(EXCEPTION(Error::LTFSDM_TAPE_NOT_EXISTS));
			pool->add(cartridge);
			return;
		}
	}

	MSG(LTFSDMX0025E, poolname);
	throw(EXCEPTION(Error::LTFSDM_POOL_NOT_EXISTS));
}

void OpenLTFSInventory::poolRemove(std::string poolname, std::string cartridgeid)

{
	std::lock_guard<std::recursive_mutex> lock(OpenLTFSInventory::mtx);

	std::shared_ptr<OpenLTFSCartridge> cartridge;

	for ( std::shared_ptr<OpenLTFSPool> pool : pools ) {
		if ( pool->getPoolName().compare(poolname) == 0 ) {
			cartridge = getCartridge(cartridgeid);
			if ( cartridge == nullptr )
				throw(EXCEPTION(Error::LTFSDM_TAPE_NOT_EXISTS));
			pool->remove(cartridge);
			return;
		}
	}

	MSG(LTFSDMX0024E, poolname);
	throw(EXCEPTION(Error::LTFSDM_POOL_NOT_EXISTS));
}


void OpenLTFSInventory::mount(std::string driveid, std::string cartridgeid)

{
	MSG(LTFSDMS0068I, cartridgeid, driveid);

	std::shared_ptr<OpenLTFSDrive> drive = inventory->getDrive(driveid);
	std::shared_ptr<OpenLTFSCartridge> cartridge = inventory->getCartridge(cartridgeid);

	assert(drive != nullptr);
	assert(cartridge != nullptr);

	assert(drive->isBusy() == true);
	assert(cartridge->getState() == OpenLTFSCartridge::MOVING);

	cartridge->Mount(driveid);

	{
		std::lock_guard<std::recursive_mutex> lock(OpenLTFSInventory::mtx);

		cartridge->update(sess);
		cartridge->setState(OpenLTFSCartridge::MOUNTED);
		TRACE(Trace::always, drive->GetObjectID());
		drive->setFree();
		drive->unsetUnmountReqNum();
	}

	MSG(LTFSDMS0069I, cartridgeid, driveid);

	std::unique_lock<std::mutex> updlock(Scheduler::mtx);
	Scheduler::cond.notify_one();
}

void OpenLTFSInventory::unmount(std::string driveid, std::string cartridgeid)

{
	MSG(LTFSDMS0070I, cartridgeid, driveid);

	std::shared_ptr<OpenLTFSDrive> drive = inventory->getDrive(driveid);
	std::shared_ptr<OpenLTFSCartridge> cartridge = inventory->getCartridge(cartridgeid);

	assert(drive != nullptr);
	assert(cartridge != nullptr);

	assert(drive->isBusy() == true);
	assert(cartridge->getState() == OpenLTFSCartridge::MOVING);
	assert(drive->get_slot() == cartridge->get_slot());

	cartridge->Unmount();

	{
		std::lock_guard<std::recursive_mutex> lock(OpenLTFSInventory::mtx);

		cartridge->update(sess);
		cartridge->setState(OpenLTFSCartridge::UNMOUNTED);
		TRACE(Trace::always, drive->GetObjectID());
		drive->setFree();
		drive->unsetUnmountReqNum();
	}

	MSG(LTFSDMS0071I, cartridgeid);

	std::unique_lock<std::mutex> updlock(Scheduler::mtx);
	Scheduler::cond.notify_one();
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

std::string OpenLTFSInventory::getMountPoint()

{
	return mountPoint;
}
