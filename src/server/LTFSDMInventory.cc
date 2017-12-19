#include "ServerIncludes.h"

LTFSDMInventory *inventory = NULL;
std::recursive_mutex LTFSDMInventory::mtx;

void LTFSDMInventory::inventorize()

{
    std::list<std::shared_ptr<Drive> > drvs;
    std::list<std::shared_ptr<Cartridge>> crts;
    std::ifstream conffile(Const::CONFIG_FILE);
    std::string line;
    int i = 0;
    int rc;

    std::lock_guard<std::recursive_mutex> lock(LTFSDMInventory::mtx);

    for (std::shared_ptr<LTFSDMDrive> d : drives)
        if (d->isBusy() == true)
            THROW(Error::DRIVE_BUSY);

    for (std::shared_ptr<LTFSDMDrive> d : drives)
        delete (d->wqp);

    drives.clear();
    cartridges.clear();

    rc = LEControl::InventoryDrive(drvs, sess);

    if (rc == -1 || drvs.size() == 0) {
        MSG(LTFSDMS0051E);
        LEControl::Disconnect(sess);
        THROW(Error::GENERAL_ERROR);
    }

    for (std::shared_ptr<Drive> i : drvs) {
        TRACE(Trace::always, i->GetObjectID());
        MSG(LTFSDMS0052I, i->GetObjectID());
        drives.push_back(std::make_shared<LTFSDMDrive>(LTFSDMDrive(*i)));
    }

    rc = LEControl::InventoryCartridge(crts, sess);

    if (rc == -1 || crts.size() == 0) {
        MSG(LTFSDMS0053E);
        LEControl::Disconnect(sess);
        THROW(Error::GENERAL_ERROR);
    }

    for (std::shared_ptr<Cartridge> c : crts) {
        if (c->get_status().compare("Not Supported") == 0)
            continue;
        TRACE(Trace::always, c->GetObjectID());
        MSG(LTFSDMS0054I, c->GetObjectID());
        cartridges.push_back(
                std::make_shared<LTFSDMCartridge>(LTFSDMCartridge(*c)));
    }

    cartridges.sort(
            [] (const std::shared_ptr<Cartridge> c1, const std::shared_ptr<Cartridge> c2)
            {   return (c1->GetObjectID().compare(c2->GetObjectID()) < 0);});

    for (std::string poolname : Server::conf.getPools()) {
        for (std::string cartridgeid : Server::conf.getPool(poolname)) {
            if (getCartridge(cartridgeid) == nullptr) {
                MSG(LTFSDMS0091W, cartridgeid, poolname);
                Server::conf.poolRemove(poolname, cartridgeid);
            } else {
                if (getCartridge(cartridgeid)->getPool().compare(poolname)
                        != 0) {
                    MSG(LTFSDMS0078I, cartridgeid, poolname);
                    getCartridge(cartridgeid)->setPool(poolname);
                }
            }
        }
    }

    for (std::shared_ptr<LTFSDMCartridge> c : cartridges) {
        c->setState(LTFSDMCartridge::TAPE_UNMOUNTED);
        for (std::shared_ptr<LTFSDMDrive> d : drives) {
            if (c->get_slot() == d->get_slot()) {
                c->setState(LTFSDMCartridge::TAPE_MOUNTED);
                break;
            }
        }
    }

    for (std::shared_ptr<LTFSDMDrive> drive : drives) {
        std::stringstream threadName;
        threadName << "pmig" << i++ << "-wq";
        drive->wqp =
                new ThreadPool<std::string, std::string, long, long,
                        Migration::mig_info_t,
                        std::shared_ptr<std::list<unsigned long>>,
                        std::shared_ptr<bool>>(&Migration::preMigrate,
                        Const::MAX_PREMIG_THREADS, threadName.str());
        drive->mtx = new std::mutex();
    }
}

LTFSDMInventory::LTFSDMInventory()

{
    std::lock_guard<std::recursive_mutex> lock(LTFSDMInventory::mtx);
    std::shared_ptr<ltfsadmin::LTFSNode> nodeInfo;
    struct stat statbuf;

    try {
        sess = LEControl::Connect("127.0.0.1", 7600);
    } catch (const std::exception& e) {
        TRACE(Trace::error, e.what());
        MSG(LTFSDMS0072E);
        THROW(Error::GENERAL_ERROR);
    }

    if (sess == nullptr) {
        MSG(LTFSDMS0072E);
        THROW(Error::GENERAL_ERROR);
    }

    try {
        nodeInfo = LEControl::InventoryNode(sess);
    } catch (const std::exception& e) {
        TRACE(Trace::error, e.what());
        MSG(LTFSDMS0072E);
        THROW(Error::GENERAL_ERROR);
    }

    try {
        mountPoint = nodeInfo->get_mount_point();
    } catch (const std::exception& e) {
        TRACE(Trace::error, e.what());
        MSG(LTFSDMS0072E);
        THROW(Error::GENERAL_ERROR);
    }

    TRACE(Trace::always, mountPoint);

    if (stat(mountPoint.c_str(), &statbuf) == -1) {
        MSG(LTFSDMS0072E);
        THROW(Error::GENERAL_ERROR, errno);
    }

    if (nodeInfo == nullptr) {
        MSG(LTFSDMS0072E);
        THROW(Error::GENERAL_ERROR);
    }

    try {
        inventorize();
    } catch (const std::exception& e) {
        TRACE(Trace::error, e.what());
        LEControl::Disconnect(sess);
        THROW(Error::GENERAL_ERROR);
    }
}

std::list<std::shared_ptr<LTFSDMDrive>> LTFSDMInventory::getDrives()

{
    std::lock_guard<std::recursive_mutex> lock(LTFSDMInventory::mtx);

    return drives;
}

std::shared_ptr<LTFSDMDrive> LTFSDMInventory::getDrive(std::string driveid)

{
    std::lock_guard<std::recursive_mutex> lock(LTFSDMInventory::mtx);

    for (std::shared_ptr<LTFSDMDrive> drive : drives)
        if (drive->GetObjectID().compare(driveid) == 0)
            return drive;

    return nullptr;
}

std::list<std::shared_ptr<LTFSDMCartridge>> LTFSDMInventory::getCartridges()

{
    std::lock_guard<std::recursive_mutex> lock(LTFSDMInventory::mtx);

    return cartridges;
}

std::shared_ptr<LTFSDMCartridge> LTFSDMInventory::getCartridge(
        std::string cartridgeid)

{
    std::lock_guard<std::recursive_mutex> lock(LTFSDMInventory::mtx);

    for (std::shared_ptr<LTFSDMCartridge> cartridge : cartridges)
        if (cartridge->GetObjectID().compare(cartridgeid) == 0)
            return cartridge;

    return nullptr;
}

void LTFSDMInventory::update(std::shared_ptr<LTFSDMDrive> drive)

{
    std::lock_guard<std::recursive_mutex> lock(LTFSDMInventory::mtx);

    drive->update(sess);
}

void LTFSDMInventory::update(std::shared_ptr<LTFSDMCartridge> cartridge)

{
    std::lock_guard<std::recursive_mutex> lock(LTFSDMInventory::mtx);

    cartridge->update(sess);
}

void LTFSDMInventory::poolCreate(std::string poolname)

{
    std::lock_guard<std::recursive_mutex> lock(LTFSDMInventory::mtx);

    try {
        Server::conf.poolCreate(poolname);
    } catch (const LTFSDMException & e) {
        MSG(LTFSDMX0023E, poolname);
        THROW(Error::POOL_EXISTS);
    }
}

void LTFSDMInventory::poolDelete(std::string poolname)

{
    std::lock_guard<std::recursive_mutex> lock(LTFSDMInventory::mtx);

    try {
        Server::conf.poolDelete(poolname);
    } catch (const LTFSDMException & e) {
        if (e.getError() == Error::CONFIG_POOL_NOT_EMPTY) {
            MSG(LTFSDMX0024E, poolname);
            THROW(Error::POOL_NOT_EMPTY);
        } else if (e.getError() == Error::CONFIG_POOL_NOT_EXISTS) {
            MSG(LTFSDMX0025E, poolname);
            THROW(Error::POOL_NOT_EXISTS);
        } else {
            MSG(LTFSDMX0033E, poolname);
            THROW(Error::GENERAL_ERROR);
        }
    }
}

void LTFSDMInventory::poolAdd(std::string poolname, std::string cartridgeid)

{
    std::lock_guard<std::recursive_mutex> lock(LTFSDMInventory::mtx);

    std::shared_ptr<LTFSDMCartridge> cartridge;

    if ((cartridge = getCartridge(cartridgeid)) == nullptr) {
        MSG(LTFSDMX0034E, cartridgeid);
        THROW(Error::TAPE_NOT_EXISTS);
    }

    if (cartridge->getPool().compare("") != 0) {
        MSG(LTFSDMX0021E, cartridgeid);
        THROW(Error::TAPE_EXISTS_IN_POOL);
    }

    try {
        Server::conf.poolAdd(poolname, cartridgeid);
    } catch (const LTFSDMException & e) {
        if (e.getError() == Error::CONFIG_POOL_NOT_EXISTS) {
            MSG(LTFSDMX0025E, poolname);
            THROW(Error::POOL_NOT_EXISTS);
        } else if (e.getError() == Error::CONFIG_TAPE_EXISTS) {
            MSG(LTFSDMX0021E, cartridgeid);
            THROW(Error::TAPE_EXISTS_IN_POOL);
        } else {
            MSG(LTFSDMX0035E, cartridgeid, poolname);
            THROW(Error::GENERAL_ERROR);
        }
    }

    cartridge->setPool(poolname);
}

void LTFSDMInventory::poolRemove(std::string poolname,
        std::string cartridgeid)

{
    std::lock_guard<std::recursive_mutex> lock(LTFSDMInventory::mtx);

    std::shared_ptr<LTFSDMCartridge> cartridge;

    if ((cartridge = getCartridge(cartridgeid)) == nullptr) {
        MSG(LTFSDMX0034E, cartridgeid);
        THROW(Error::TAPE_NOT_EXISTS);
    }

    if (cartridge->getPool().compare(poolname) != 0) {
        MSG(LTFSDMX0021E, cartridgeid);
        THROW(Error::TAPE_NOT_EXISTS_IN_POOL);
    }

    try {
        Server::conf.poolRemove(poolname, cartridgeid);
    } catch (const LTFSDMException& e) {
        if (e.getError() == Error::CONFIG_POOL_NOT_EXISTS) {
            MSG(LTFSDMX0025E, poolname);
            THROW(Error::POOL_NOT_EXISTS);
        } else if (e.getError() == Error::CONFIG_TAPE_NOT_EXISTS) {
            MSG(LTFSDMX0022E, cartridgeid, poolname);
            THROW(Error::TAPE_NOT_EXISTS);
        } else {
            MSG(LTFSDMX0036E, cartridgeid, poolname);
            THROW(Error::GENERAL_ERROR);
        }
    }
}

void LTFSDMInventory::mount(std::string driveid, std::string cartridgeid)

{
    MSG(LTFSDMS0068I, cartridgeid, driveid);

    std::shared_ptr<LTFSDMDrive> drive = inventory->getDrive(driveid);
    std::shared_ptr<LTFSDMCartridge> cartridge = inventory->getCartridge(
            cartridgeid);

    assert(drive != nullptr);
    assert(cartridge != nullptr);

    assert(drive->isBusy() == true);
    assert(cartridge->getState() == LTFSDMCartridge::TAPE_MOVING);

    cartridge->Mount(driveid);

    {
        std::lock_guard<std::recursive_mutex> lock(LTFSDMInventory::mtx);

        cartridge->update(sess);
        cartridge->setState(LTFSDMCartridge::TAPE_MOUNTED);
        TRACE(Trace::always, drive->GetObjectID());
        drive->setFree();
        drive->unsetUnmountReqNum();
    }

    MSG(LTFSDMS0069I, cartridgeid, driveid);

    std::unique_lock<std::mutex> updlock(Scheduler::mtx);
    Scheduler::cond.notify_one();
}

void LTFSDMInventory::unmount(std::string driveid, std::string cartridgeid)

{
    MSG(LTFSDMS0070I, cartridgeid, driveid);

    std::shared_ptr<LTFSDMDrive> drive = inventory->getDrive(driveid);
    std::shared_ptr<LTFSDMCartridge> cartridge = inventory->getCartridge(
            cartridgeid);

    assert(drive != nullptr);
    assert(cartridge != nullptr);

    assert(drive->isBusy() == true);
    assert(cartridge->getState() == LTFSDMCartridge::TAPE_MOVING);
    assert(drive->get_slot() == cartridge->get_slot());

    cartridge->Unmount();

    {
        std::lock_guard<std::recursive_mutex> lock(LTFSDMInventory::mtx);

        cartridge->update(sess);
        cartridge->setState(LTFSDMCartridge::TAPE_UNMOUNTED);
        TRACE(Trace::always, drive->GetObjectID());
        drive->setFree();
        drive->unsetUnmountReqNum();
    }

    MSG(LTFSDMS0071I, cartridgeid);

    std::unique_lock<std::mutex> updlock(Scheduler::mtx);
    Scheduler::cond.notify_one();
}

void LTFSDMInventory::format(std::string cartridgeid)

{
}

void LTFSDMInventory::check(std::string cartridgeid)

{
}

LTFSDMInventory::~LTFSDMInventory()

{
    try {
        MSG(LTFSDMS0099I);

        for (std::shared_ptr<LTFSDMDrive> drive : drives)
            delete (drive->wqp);

        LEControl::Disconnect(sess);
    } catch (const std::exception& e) {
        TRACE(Trace::error, e.what());
        Server::forcedTerminate = true;
        kill(getpid(), SIGUSR1);
    }
}

std::string LTFSDMInventory::getMountPoint()

{
    return mountPoint;
}
