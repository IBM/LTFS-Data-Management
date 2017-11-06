#pragma once

class OpenLTFSDrive: public ltfsadmin::Drive
{
private:
    bool busy;
    int umountReqNum;
    DataBase::operation toUnBlock;
public:
    std::mutex *mtx;
    ThreadPool<std::string, std::string, long, long, Migration::mig_info_t,
            std::shared_ptr<std::list<unsigned long>>, std::shared_ptr<bool>> *wqp;
    OpenLTFSDrive(ltfsadmin::Drive drive);
    ~OpenLTFSDrive();
    void update(std::shared_ptr<LTFSAdminSession> sess);
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

class OpenLTFSCartridge: public ltfsadmin::Cartridge
{
private:
    unsigned long inProgress;
    std::string pool;
    bool requested;
public:
    enum state_t
    {
        TAPE_INUSE, TAPE_MOUNTED, TAPE_MOVING, TAPE_UNMOUNTED, TAPE_INVALID, TAPE_UNKNOWN
    } state;
    OpenLTFSCartridge(ltfsadmin::Cartridge cartridge) :
            ltfsadmin::Cartridge(cartridge), inProgress(0), pool(""), requested(
                    false), state(OpenLTFSCartridge::TAPE_UNKNOWN)
    {
    }
    void update(std::shared_ptr<LTFSAdminSession> sess);
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

class OpenLTFSInventory
{
private:
    std::list<std::shared_ptr<OpenLTFSDrive>> drives;
    std::list<std::shared_ptr<OpenLTFSCartridge>> cartridges;
    std::shared_ptr<ltfsadmin::LTFSAdminSession> sess;
    std::string mountPoint;
public:
    OpenLTFSInventory();
    ~OpenLTFSInventory();

    static std::recursive_mutex mtx;

    void inventorize();

    std::list<std::shared_ptr<OpenLTFSDrive>> getDrives();
    std::shared_ptr<OpenLTFSDrive> getDrive(std::string driveid);
    std::list<std::shared_ptr<OpenLTFSCartridge>> getCartridges();
    std::shared_ptr<OpenLTFSCartridge> getCartridge(std::string cartridgeid);

    void update(std::shared_ptr<OpenLTFSDrive>);
    void update(std::shared_ptr<OpenLTFSCartridge>);

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

extern OpenLTFSInventory *inventory;
