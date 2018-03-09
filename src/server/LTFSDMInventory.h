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
#pragma once

#include <ltfs/ltfsadminlib/LTFSAdminLog.h>
#include <ltfs/ltfsadminlib/LTFSAdminSession.h>
#include <ltfs/ltfsadminlib/InternalError.h>
#include <ltfs/ltfsadminlib/Drive.h>
#include <ltfs/ltfsadminlib/Cartridge.h>
#include <ltfs/ltfsadminlib/LTFSNode.h>

using namespace ltfsadmin;

class LTFSDMDrive
{
private:
    boost::shared_ptr<Drive> drive;
    bool busy;
    int umountReqNum;
    DataBase::operation toUnBlock;
public:
    std::mutex *mtx;
    ThreadPool<std::string, std::string, long, long, Migration::mig_info_t,
            std::shared_ptr<std::list<unsigned long>>, std::shared_ptr<bool>> *wqp;
    LTFSDMDrive(boost::shared_ptr<Drive> d);
    ~LTFSDMDrive();
    boost::shared_ptr<Drive> get_le()
    {
        return drive;
    }
    void update();
    bool isBusy();
    void setBusy();
    void setFree();
    void setUnmountReqNum(int reqnum);
    int getUnmountReqNum();
    void unsetUnmountReqNum();
    void setToUnblock(DataBase::operation op);
    DataBase::operation getToUnblock();
    void clearToUnblock();
};

class LTFSDMCartridge
{
private:
    boost::shared_ptr<Cartridge> cart;
    unsigned long inProgress;
    std::string pool;
    bool requested;
public:
    enum state_t
    {
        TAPE_INUSE,
        TAPE_MOUNTED,
        TAPE_MOVING,
        TAPE_UNMOUNTED,
        TAPE_INVALID,
        TAPE_UNKNOWN
    } state;
    LTFSDMCartridge(boost::shared_ptr<Cartridge> c);
    boost::shared_ptr<Cartridge> get_le()
    {
        return cart;
    }
    void update();
    void setInProgress(unsigned long size);
    unsigned long getInProgress();
    void setPool(std::string _pool);
    std::string getPool();
    void setState(state_t _state);
    state_t getState();
    bool isRequested();
    void setRequested();
    void unsetRequested();
};

class LTFSDMInventory
{
private:
    std::list<std::shared_ptr<LTFSDMDrive>> drives;
    std::list<std::shared_ptr<LTFSDMCartridge>> cartridges;
    boost::shared_ptr<LTFSAdminSession> sess;
    boost::shared_ptr<LTFSNode> node;
    std::string mountPoint;

    void connect(std::string node_addr, unsigned short int port_num);
    void disconnect();
    void getNode();

    void addDrive(std::string serial);
    void remDrive(boost::shared_ptr<Drive> drive);
    void lookupDrives(bool assigned_only = true, bool force = false);
    void addCartridge(std::string barcode, std::string drive_serial);
    void remCartridge(boost::shared_ptr<Cartridge> cartridge,
            bool keep_on_drive = false);
    void lookupCartridges(bool assigned_only = true, bool force = false);
public:
    LTFSDMInventory();
    ~LTFSDMInventory();

    static std::recursive_mutex mtx;

    boost::shared_ptr<Drive> lookupDrive(std::string id, bool force = false);
    boost::shared_ptr<Cartridge> lookupCartridge(std::string id, bool force =
            false);
    void updateCartridge(std::string tapeId);
    void inventorize();

    std::list<std::shared_ptr<LTFSDMDrive>> getDrives();
    std::shared_ptr<LTFSDMDrive> getDrive(std::string driveid);
    std::list<std::shared_ptr<LTFSDMCartridge>> getCartridges();
    std::shared_ptr<LTFSDMCartridge> getCartridge(std::string cartridgeid);

    void update(std::shared_ptr<LTFSDMDrive>);
    void update(std::shared_ptr<LTFSDMCartridge>);

    void poolCreate(std::string poolname);
    void poolDelete(std::string poolname);
    void poolAdd(std::string poolname, std::string cartridgeid);
    void poolRemove(std::string poolname, std::string cartridgeid);

    void mount(std::string driveid, std::string cartridgeid);
    void unmount(std::string driveid, std::string cartridgeid);
    void format(std::string cartridgeid);
    void check(std::string cartridgeid);

    std::string getMountPoint();
};

extern LTFSDMInventory *inventory;
