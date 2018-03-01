/*******************************************************************************
 * Copyright 2018 IBM Corp. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *  https://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 *******************************************************************************/
#include "ServerIncludes.h"

using namespace ltfsadmin;

LTFSDMInventory *inventory = NULL;
std::recursive_mutex LTFSDMInventory::mtx;

void LTFSDMInventory::connect(std::string node_addr, unsigned short int port_num)
{
    sess = boost::shared_ptr<LTFSAdminSession>(new LTFSAdminSession(node_addr, port_num));

    if (sess) {
        MSG(LTFSDML0001I, node_addr.c_str(), port_num);
        try {
            sess->Connect();
            sess->SessionLogin();
            MSG(LTFSDML0002I, node_addr.c_str(), port_num, sess->get_fd());
        } catch (AdminLibException& e) {
            MSG(LTFSDML0018E, node_addr.c_str());
            sess = boost::shared_ptr<LTFSAdminSession>();
        }
    }
}

void LTFSDMInventory::disconnect()
{
    if (sess) {
        MSG(LTFSDML0005I, sess->get_server().c_str(), sess->get_port(), sess->get_fd());
        try {
            try {
                sess->SessionLogout();
            } catch (AdminLibException& e) {
                MSG(LTFSDML0019E);
            }
            sess->Disconnect();
            MSG(LTFSDML0006I, sess->get_server().c_str(), sess->get_port());
        } catch ( AdminLibException& e ) {
            MSG(LTFSDML0019E);
        }
    }
}

void LTFSDMInventory::getNode()
{
    node = boost::shared_ptr<LTFSNode>();

    if (sess && sess->is_alived()) {
        try {
            std::list<boost::shared_ptr <LTFSNode> > node_list;
            sess->SessionInventory(node_list);
            MSG(LTFSDML0007I, "node", sess->get_server().c_str(), sess->get_port(), sess->get_fd());
            if (node_list.size() == 1) {
                MSG(LTFSDML0008I, "node", sess->get_server().c_str(), sess->get_port(), sess->get_fd());
                node = *(node_list.begin());
            } else
                MSG(LTFSDML0009E, node_list.size(), sess->get_server().c_str(), sess->get_port(), sess->get_fd());
        } catch ( AdminLibException& e ) {
            MSG(LTFSDML0010E, "Inventory", "node", sess->get_server().c_str(), sess->get_port(), sess->get_fd(), e.what());
            node = boost::shared_ptr<LTFSNode>();
        }
    }
}

boost::shared_ptr<Drive> LTFSDMInventory::lookupDrive(std::string id, bool force)
{
    boost::shared_ptr<Drive> drive = boost::shared_ptr<Drive>();

    if (sess && sess->is_alived()) {
        std::string type = "drive (" + id + ")";
        MSG(LTFSDML0007I, type.c_str(), sess->get_server().c_str(), sess->get_port(),
                sess->get_fd());
        try {
            std::list<boost::shared_ptr <Drive> > drive_list;
            sess->SessionInventory(drive_list, id, force);

            for (boost::shared_ptr<Drive> d : drive_list) {
                try {
                    if (id == d->GetObjectID()) {
                        drive = d;
                        MSG(LTFSDML0008I, type.c_str(), sess->get_server().c_str(), sess->get_port(), sess->get_fd());
                    }
                } catch ( InternalError& ie ) {
                    if (ie.GetID() != "031E") {
                        throw(ie);
                    }
                    /* Just ignore 0-byte ID objects */
                }
            }
            if (!drive)
                MSG(LTFSDML0011W, type.c_str(), sess->get_server().c_str(), sess->get_port(), sess->get_fd());
        } catch ( AdminLibException& e ) {
            MSG(LTFSDML0010E, "Inventory", type.c_str(), sess->get_server().c_str(), sess->get_port(), sess->get_fd(), e.what());
            drive = boost::shared_ptr<Drive>();
        }
    }

    return drive;
}

void LTFSDMInventory::addDrive(std::string serial)
{
    if (sess && sess->is_alived()) {
        std::string type = "drive (" + serial + ")";
        MSG(LTFSDML0012I, type.c_str(), sess->get_server().c_str(), sess->get_port(), sess->get_fd());
        try {
            boost::shared_ptr<Drive> d = lookupDrive(serial);
            if (!d) {
                /* Refresh inventory and retry */
                d = lookupDrive(serial);
            }

            if (d) {
                d->Add();
                MSG(LTFSDML0013I, type.c_str(), sess->get_server().c_str(), sess->get_port(), sess->get_fd());
                return;
            } else
                MSG(LTFSDML0011W, type.c_str(), sess->get_server().c_str(),
                        sess->get_port(), sess->get_fd());
        } catch ( AdminLibException& e ) {
            MSG(LTFSDML0010E, "Assign", type.c_str(), sess->get_server().c_str(), sess->get_port(), sess->get_fd(), e.what());
            THROW(Error::GENERAL_ERROR);
        }
    }

    THROW(Error::GENERAL_ERROR);
}

void LTFSDMInventory::remDrive(boost::shared_ptr<Drive> drive)
{
    if (drive) {
        std::string type = "drive (" + drive->GetObjectID() + ")";
        MSG(LTFSDML0014I, type.c_str(), sess->get_server().c_str(), sess->get_port(), sess->get_fd());
        try {
            drive->Remove();
            MSG(LTFSDML0015I, type.c_str(), sess->get_server().c_str(), sess->get_port(), sess->get_fd());
            return;
        } catch ( AdminLibException& e ) {
            std::string msg_oob = e.GetOOBError();
            if (msg_oob == "LTFSI1090E") {
                MSG(LTFSDML0016I, type.c_str(), sess->get_server().c_str(), sess->get_port(), sess->get_fd());
                return;
            } else {
                MSG(LTFSDML0010E, "Unassign", type.c_str(), sess->get_server().c_str(), sess->get_port(), sess->get_fd(), e.what());
                THROW(Error::GENERAL_ERROR);
            }
        }
    }

    THROW(Error::GENERAL_ERROR);
}

void LTFSDMInventory::lookupDrives(bool assigned_only, bool force)
{
    std::list<boost::shared_ptr<Drive> > drvs;

    if (sess && sess->is_alived()) {
        try {
            if (assigned_only) {
                MSG(LTFSDML0007I, "assigned drive", sess->get_server().c_str(), sess->get_port(), sess->get_fd());
                sess->SessionInventory(drvs, "__ACTIVE_ONLY__", force);
                MSG(LTFSDML0008I, "assigned drive", sess->get_server().c_str(), sess->get_port(), sess->get_fd());
            } else {
                MSG(LTFSDML0007I, "drive", sess->get_server().c_str(), sess->get_port(), sess->get_fd());
                sess->SessionInventory(drvs, "", force);
                MSG(LTFSDML0008I, "drive", sess->get_server().c_str(), sess->get_port(), sess->get_fd());
            }
        } catch ( AdminLibException& e ) {
            MSG(LTFSDML0010E, "Inventory", "drive", sess->get_server().c_str(), sess->get_port(), sess->get_fd(), e.what());
            drives.clear();
            THROW(Error::GENERAL_ERROR);
        }

        if (drvs.size() == 0) {
            MSG(LTFSDMS0051E);
            disconnect();
            THROW(Error::GENERAL_ERROR);
        }

        for (boost::shared_ptr<Drive> d : drvs) {
            TRACE(Trace::always, d->GetObjectID());
            MSG(LTFSDMS0052I, d->GetObjectID());
            drives.push_back(std::make_shared<LTFSDMDrive>(d));
        }

        return;
    }

    THROW(Error::GENERAL_ERROR);
}


boost::shared_ptr<Cartridge> LTFSDMInventory::lookupCartridge(std::string id, bool force)
{
    boost::shared_ptr<Cartridge> cart = boost::shared_ptr<Cartridge>();

    if (sess && sess->is_alived()) {
        std::string type = "tape (" + id + ")";
        MSG(LTFSDML0007I, type.c_str(), sess->get_server().c_str(), sess->get_port(), sess->get_fd());
        try {
            std::list<boost::shared_ptr <Cartridge> > cartridge_list;
            sess->SessionInventory(cartridge_list, id);

            if (cartridge_list.size() == 1) {
                MSG(LTFSDML0008I, type.c_str(), sess->get_server().c_str(), sess->get_port(), sess->get_fd());
                cart = *(cartridge_list.begin());
            } else
                MSG(LTFSDML0017E, sess->get_server().c_str(), sess->get_port(), sess->get_fd());
        } catch ( AdminLibException& e ) {
            MSG(LTFSDML0010E, "Inventory", type.c_str(), sess->get_server().c_str(), sess->get_port(), sess->get_fd(), e.what());
            cart = boost::shared_ptr<Cartridge>();
        }
    }

    return cart;
}

void LTFSDMInventory::addCartridge(std::string barcode, std::string drive_serial)
{
    if (sess && sess->is_alived()) {
        std::string type = "tape (" + barcode + ")";
        MSG(LTFSDML0012I, type.c_str(), sess->get_server().c_str(), sess->get_port(), sess->get_fd());
        try {
            boost::shared_ptr<Cartridge> cart = lookupCartridge(barcode);
            if (!cart) {
                /* Refresh inventory and retry */
                cart = lookupCartridge(barcode);
            }

            if (cart) cart->Add();

            /* Refresh inventory and mount if it is UNKNOWN status */
            cart = lookupCartridge(barcode);
            if (cart) {
                if (drive_serial.length() && cart->get_status().compare("NOT_MOUNTED_YET") == 0 ) {
                    //StatusConv::get_cart_value(c->get_status()) == "NOT_MOUNTED_YET" ) {  /* needs to be checked */
                    cart->Mount(drive_serial);
                    cart->Unmount();
                }
                MSG(LTFSDML0013I, type.c_str(), sess->get_server().c_str(), sess->get_port(), sess->get_fd());
                return;
            } else
                MSG(LTFSDML0011W, type.c_str(), sess->get_server().c_str(), sess->get_port(), sess->get_fd());

        } catch ( AdminLibException& e ) {
            MSG(LTFSDML0010E, "Assign", type.c_str(), sess->get_server().c_str(), sess->get_port(), sess->get_fd(), e.what());
            THROW(Error::GENERAL_ERROR);
        }
    }

    THROW(Error::GENERAL_ERROR);
}

void LTFSDMInventory::remCartridge(boost::shared_ptr<Cartridge> cart, bool keep_on_drive)
{
    if (cart) {
        if (sess && sess->is_alived()) {
            std::string type = "tape (" + cart->GetObjectID() + ")";
            MSG(LTFSDML0014I, type.c_str(), sess->get_server().c_str(), sess->get_port(), sess->get_fd());
            try {
                cart->Remove(true, keep_on_drive);
                MSG(LTFSDML0015I, type.c_str(), sess->get_server().c_str(), sess->get_port(), sess->get_fd());
                return;
            } catch ( AdminLibException& e ) {
                std::string msg_oob = e.GetOOBError();
                if (msg_oob == "LTFSI1090E") {
                    MSG(LTFSDML0016I, type.c_str(), sess->get_server().c_str(), sess->get_port(), sess->get_fd());
                    return;
                } else {
                    MSG(LTFSDML0010E, "Unassign", type.c_str(), sess->get_server().c_str(), sess->get_port(), sess->get_fd(), e.what());
                    THROW(Error::GENERAL_ERROR);
                }
            }
        }
    }

    THROW(Error::GENERAL_ERROR);
}


void LTFSDMInventory::lookupCartridges(bool assigned_only, bool force)
{
    std::list<boost::shared_ptr<Cartridge>> crts;

    if (sess && sess->is_alived()) {
        try {
            if (assigned_only) {
                MSG(LTFSDML0007I, "assigned tape", sess->get_server().c_str(), sess->get_port(), sess->get_fd());
                sess->SessionInventory(crts, "__ACTIVE_ONLY__", force);
                MSG(LTFSDML0008I, "assigned tape", sess->get_server().c_str(), sess->get_port(), sess->get_fd());
            } else {
                MSG(LTFSDML0007I, "tape", sess->get_server().c_str(), sess->get_port(), sess->get_fd());
                sess->SessionInventory(crts, "", force);
                MSG(LTFSDML0008I, "tape", sess->get_server().c_str(), sess->get_port(), sess->get_fd());
            }
        } catch ( AdminLibException& e ) {
            MSG(LTFSDML0010E, "Inventory", "tape", sess->get_server().c_str(), sess->get_port(), sess->get_fd(), e.what());
            cartridges.clear();
            THROW(Error::GENERAL_ERROR);
        }

        if (crts.size() == 0) {
            MSG(LTFSDMS0053E);
            disconnect();
            THROW(Error::GENERAL_ERROR);
        }

        for (boost::shared_ptr<Cartridge> c : crts) {
            if (c->get_status().compare("Not Supported") == 0)
                continue;
            TRACE(Trace::always, c->GetObjectID());
            MSG(LTFSDMS0054I, c->GetObjectID());
            cartridges.push_back(std::make_shared<LTFSDMCartridge>(c));
        }

        return;
    }

    THROW(Error::GENERAL_ERROR);
}

void LTFSDMInventory::updateCartridge(std::string tapeId)

{
    if (sess && sess->is_alived()) {
        try {
            std::lock_guard<std::recursive_mutex> lock(LTFSDMInventory::mtx);

            for ( std::shared_ptr<LTFSDMCartridge> c : cartridges) {
                if ( c->get_le()->GetObjectID().compare(tapeId) == 0 ) {
                    MSG(LTFSDMS0106I, tapeId);

                    c->update();

                    c->setState(LTFSDMCartridge::TAPE_UNMOUNTED);
                    for (std::shared_ptr<LTFSDMDrive> d : drives) {
                        TRACE(Trace::always, tapeId, c->get_le()->get_slot(), d->get_le()->get_slot());
                        if (c->get_le()->get_slot() == d->get_le()->get_slot()) {
                            c->setState(LTFSDMCartridge::TAPE_MOUNTED);
                            break;
                        }
                    }

                    break;
                }
            }

        } catch (const LTFSDMException& e) {
            MSG(LTFSDMS0108E, tapeId, e.what());
        } catch (const std::exception& e) {
            MSG(LTFSDMS0108E, tapeId, e.what());
        }
    }
}

void LTFSDMInventory::inventorize()

{
    std::ifstream conffile(Const::CONFIG_FILE);
    std::string line;
    int i = 0;

    std::lock_guard<std::recursive_mutex> lock(LTFSDMInventory::mtx);

    for (std::shared_ptr<LTFSDMDrive> d : drives) {
        if (d->isBusy() == true) {
            MSG(LTFSDMS0103I, d->get_le()->GetObjectID());
            THROW(Error::DRIVE_BUSY);
        }
    }

    for (std::shared_ptr<LTFSDMDrive> d : drives)
        delete (d->wqp);

    drives.clear();
    cartridges.clear();

    lookupDrives();

    lookupCartridges();

    cartridges.sort(
            [] (const std::shared_ptr<LTFSDMCartridge> c1, const std::shared_ptr<LTFSDMCartridge> c2)
            {   return (c1->get_le()->GetObjectID().compare(c2->get_le()->GetObjectID()) < 0);});

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
            if (c->get_le()->get_slot() == d->get_le()->get_slot()) {
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
                        std::shared_ptr<bool>>(&Migration::transferData,
                        Const::MAX_PREMIG_THREADS, threadName.str());
        drive->mtx = new std::mutex();
    }
}

LTFSDMInventory::LTFSDMInventory()

{
    std::lock_guard<std::recursive_mutex> lock(LTFSDMInventory::mtx);
    struct stat statbuf;

    try {
        connect(Const::LTFSLE_HOST, Const::LTFSLE_PORT);
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
        getNode();
    } catch (const std::exception& e) {
        TRACE(Trace::error, e.what());
        MSG(LTFSDMS0072E);
        THROW(Error::GENERAL_ERROR);
    }

    try {
        mountPoint = node->get_mount_point();
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

    if (node == nullptr) {
        MSG(LTFSDMS0072E);
        THROW(Error::GENERAL_ERROR);
    }

    try {
        inventorize();
    } catch (const std::exception& e) {
        TRACE(Trace::error, e.what());
        MSG(LTFSDMS0101E, e.what());
        disconnect();
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
        if (drive->get_le()->GetObjectID().compare(driveid) == 0)
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
        if (cartridge->get_le()->GetObjectID().compare(cartridgeid) == 0)
            return cartridge;

    return nullptr;
}

void LTFSDMInventory::update(std::shared_ptr<LTFSDMDrive> drive)

{
    std::lock_guard<std::recursive_mutex> lock(LTFSDMInventory::mtx);

    drive->update();
}

void LTFSDMInventory::update(std::shared_ptr<LTFSDMCartridge> cartridge)

{
    std::lock_guard<std::recursive_mutex> lock(LTFSDMInventory::mtx);

    cartridge->update();
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
    std::string error;

    MSG(LTFSDMS0068I, cartridgeid, driveid);

    std::shared_ptr<LTFSDMDrive> drive = inventory->getDrive(driveid);
    std::shared_ptr<LTFSDMCartridge> cartridge = inventory->getCartridge(
            cartridgeid);

    assert(drive != nullptr);
    assert(cartridge != nullptr);

    assert(drive->isBusy() == true);
    assert(cartridge->getState() == LTFSDMCartridge::TAPE_MOVING);

    try {
        cartridge->get_le()->Mount(driveid);

        std::lock_guard<std::recursive_mutex> lock(LTFSDMInventory::mtx);

        cartridge->update();
        cartridge->setState(LTFSDMCartridge::TAPE_MOUNTED);
        TRACE(Trace::always, drive->get_le()->GetObjectID());
        drive->setFree();
        drive->unsetUnmountReqNum();
        MSG(LTFSDMS0069I, cartridgeid, driveid);
    } catch (AdminLibException& e) {
        error = e.GetID();
        MSG(LTFSDMS0100E, cartridgeid, e.what());
        drive->setFree();
        try {
            inventorize();
        } catch (const LTFSDMException& e) {
            MSG(LTFSDMS0101E, e.what());
        } catch (const std::exception& e) {
            MSG(LTFSDMS0101E, e.what());
        }
        if ( error.compare("076E") == 0 ) {
            MSG(LTFSDMS0107I, Const::WAIT_TAPE_MOUNT);
            sleep(Const::WAIT_TAPE_MOUNT);
        }
    } catch (std::exception& e) {
           MSG(LTFSDMS0100E, cartridgeid, e.what());
           drive->setFree();
           try {
               inventorize();
           } catch (const LTFSDMException& e) {
               MSG(LTFSDMS0101E, e.what());
           } catch (const std::exception& e) {
               MSG(LTFSDMS0101E, e.what());
           }
    }

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
    assert(drive->get_le()->get_slot() == cartridge->get_le()->get_slot());

    try {
        cartridge->get_le()->Unmount();

        std::lock_guard<std::recursive_mutex> lock(LTFSDMInventory::mtx);

        cartridge->update();
        cartridge->setState(LTFSDMCartridge::TAPE_UNMOUNTED);
        TRACE(Trace::always, drive->get_le()->GetObjectID());
        drive->setFree();
        drive->unsetUnmountReqNum();
        MSG(LTFSDMS0071I, cartridgeid);
    } catch (AdminLibException& e) {
            MSG(LTFSDMS0100E, cartridgeid, e.what());
            sleep(1);
            drive->setFree();
            try {
                inventorize();
            } catch (const LTFSDMException& e) {
                MSG(LTFSDMS0101E, e.what());
            } catch (const std::exception& e) {
                MSG(LTFSDMS0101E, e.what());
            }
    } catch (std::exception& e) {
           MSG(LTFSDMS0100E, cartridgeid, e.what());
           sleep(1);
           drive->setFree();
           try {
               inventorize();
           } catch (const LTFSDMException& e) {
               MSG(LTFSDMS0101E, e.what());
           } catch (const std::exception& e) {
               MSG(LTFSDMS0101E, e.what());
           }
    }

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

        disconnect();
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
