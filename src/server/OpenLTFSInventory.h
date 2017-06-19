#ifndef _INVENTORY_H
#define _INVENTORY_H

class OpenLTFSDrive : public ltfsadmin::Drive {
private:
	bool busy;
	int umountReqNum;
	DataBase::operation toUnBlock;
public:
	WorkQueue<std::string, std::string, long, long, Migration::mig_info_t,
		std::shared_ptr<std::list<unsigned long>>, std::shared_ptr<bool>> *wqp;
	OpenLTFSDrive(ltfsadmin::Drive drive) : ltfsadmin::Drive(drive), busy(false),
											umountReqNum(Const::UNSET), toUnBlock(DataBase::NOOP) {}
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

class OpenLTFSCartridge : public ltfsadmin::Cartridge {
private:
	unsigned long inProgress;
	std::string pool;
	bool requested;
public:
	enum state_t {
		INUSE,
		MOUNTED,
		MOVING,
		UNMOUNTED,
		INVALID,
		UNKNOWN
	} state;
	OpenLTFSCartridge(ltfsadmin::Cartridge cartridge) : ltfsadmin::Cartridge(cartridge), inProgress(0),
		pool(""), requested(false), state(OpenLTFSCartridge::UNKNOWN) {}
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

class OpenLTFSPool {
private:
	std::string poolName;
	std::list<std::shared_ptr<OpenLTFSCartridge>> cartridges;
public:
	OpenLTFSPool(std::string _poolName);
	std::string getPoolName();
	void add(std::shared_ptr<OpenLTFSCartridge> cartridge);
	void remove(std::shared_ptr<OpenLTFSCartridge> cartridge);
	std::list<std::shared_ptr<OpenLTFSCartridge>> getCartridges();
};

class OpenLTFSInventory {
private:
	std::list<std::shared_ptr<OpenLTFSDrive>> drives;
	std::list<std::shared_ptr<OpenLTFSCartridge>> cartridges;
	std::list<std::shared_ptr<OpenLTFSPool>> pools;
	std::shared_ptr<ltfsadmin::LTFSAdminSession> sess;
public:
	OpenLTFSInventory();
	~OpenLTFSInventory();

	static std::recursive_mutex mtx;

	void writePools();
	void inventorize();

	std::list<std::shared_ptr<OpenLTFSDrive>> getDrives();
	std::shared_ptr<OpenLTFSDrive> getDrive(std::string driveid);
	std::list<std::shared_ptr<OpenLTFSCartridge>> getCartridges();
	std::shared_ptr<OpenLTFSCartridge> getCartridge(std::string cartridgeid);
	std::list<std::shared_ptr<OpenLTFSPool>> getPools();
	std::shared_ptr<OpenLTFSPool> getPool(std::string poolname);

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
};

extern OpenLTFSInventory *inventory;

#endif /* _INVENTORY_H */
