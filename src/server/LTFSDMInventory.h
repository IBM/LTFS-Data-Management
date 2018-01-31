#pragma once

#include <ltfs/ltfsadminlib/LTFSAdminLog.h>
#include <ltfs/ltfsadminlib/LTFSAdminSession.h>
#include <ltfs/ltfsadminlib/InternalError.h>
#include <ltfs/ltfsadminlib/Drive.h>
#include <ltfs/ltfsadminlib/Cartridge.h>
#include <ltfs/ltfsadminlib/LTFSNode.h>

using namespace ltfsadmin;

class LTFSDMDrive: public Drive
{
private:
    bool busy;
    int umountReqNum;
    DataBase::operation toUnBlock;
public:
    std::mutex *mtx;
    ThreadPool<std::string, std::string, long, long, Migration::mig_info_t,
            std::shared_ptr<std::list<unsigned long>>, std::shared_ptr<bool>> *wqp;
    LTFSDMDrive(Drive drive);
    ~LTFSDMDrive();
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

class LTFSDMCartridge: public Cartridge
{
private:
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
    LTFSDMCartridge(Cartridge cartridge) :
            Cartridge(cartridge), inProgress(0), pool(""), requested(
                    false), state(LTFSDMCartridge::TAPE_UNKNOWN)
    {
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

    void connect(std::string node_addr, uint16_t port_num);
    void disconnect();
    void getNode();

    boost::shared_ptr<Drive> lookupDrive(std::string id, bool force = false);
    void addDrive(std::string serial);
    void remDrive(boost::shared_ptr<Drive> drive);
    void lookupDrives(bool assigned_only = true, bool force = false);
    boost::shared_ptr<Cartridge> lookupCartridge(std::string id, bool force = false);
    void addCartridge(std::string barcode, std::string drive_serial);
    void remCartridge(boost::shared_ptr<Cartridge> cartridge, bool keep_on_drive = false);
    void lookupCartridges(bool assigned_only = true, bool force = false);
public:
    LTFSDMInventory();
    ~LTFSDMInventory();

    static std::recursive_mutex mtx;

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
