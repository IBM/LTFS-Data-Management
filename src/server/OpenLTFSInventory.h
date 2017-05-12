#ifndef _INVENTORY_H
#define _INVENTORY_H

class OpenLTFSDrive : public ltfsadmin::Drive {
private:
	bool busy;
	WorkQueue<std::string, long, long, Migration::mig_info_t> *wqp;
public:
	OpenLTFSDrive(ltfsadmin::Drive drive) : ltfsadmin::Drive(drive), busy(false) {}
	void update(std::shared_ptr<LTFSAdminSession> sess);
	bool isBusy() { return busy; }
	void setBusy() { busy = true; }
	void setFree() { busy = false; }
};

class OpenLTFSCartridge : public ltfsadmin::Cartridge {
private:
	unsigned long inProgress;
	std::string pool;
public:
	enum state_t {
		INUSE,
		MOUNTED,
		MOVING,
		UNMOUNTED,
		INVALID,
		UNKNOWN
	} state;
	OpenLTFSCartridge(ltfsadmin::Cartridge cartridge) : ltfsadmin::Cartridge(cartridge), inProgress(0), pool(""), state(OpenLTFSCartridge::UNKNOWN) {}
	void update(std::shared_ptr<LTFSAdminSession> sess);
	void setInProgress(unsigned long size) { inProgress = size; }
	unsigned long getInProgress() { return inProgress; }
	void setPool(std::string _pool) { pool = _pool; }
	std::string getPool() { return pool; }
	void setState(state_t _state) { state = _state; }
	state_t getState() { return state; }
};

class OpenLTFSPool {
private:
	std::string poolName;
	std::list<std::shared_ptr<OpenLTFSCartridge>> cartridges;
public:
	OpenLTFSPool(std::string _poolName);
	std::string getPoolName() { return poolName; }
	void add(std::shared_ptr<OpenLTFSCartridge> cartridge);
	void remove(std::shared_ptr<OpenLTFSCartridge> cartridge);
	std::list<OpenLTFSCartridge> getCartridges();
};

class OpenLTFSInventory {
private:
	std::list<std::shared_ptr<OpenLTFSDrive>> drives;
	std::list<std::shared_ptr<OpenLTFSCartridge>> cartridges;
	std::list<std::shared_ptr<OpenLTFSPool>> pools;
	std::shared_ptr<ltfsadmin::LTFSAdminSession> sess;
	std::mutex mtx;
	std::unique_lock<std::mutex> lck;
	void writePools();
public:
	OpenLTFSInventory();
	~OpenLTFSInventory();
	void lock() { lck.lock(); }
	void unlock() { lck.unlock(); }
	void reinventorize();

	std::list<OpenLTFSDrive> getDrives();
	std::shared_ptr<OpenLTFSDrive> getDrive(std::string driveid);
	std::list<OpenLTFSCartridge> getCartridges();
	std::shared_ptr<OpenLTFSCartridge> getCartridge(std::string cartridgeid);
	std::list<OpenLTFSPool> getPools();
	std::shared_ptr<OpenLTFSPool> getPool(std::string poolname);

	void poolCreate(std::string poolname);
	void poolDelete(std::string poolname);
	void poolAdd(std::string poolname, std::string cartridgeid);
	void poolRemove(std::string poolname, std::string cartridgeid);

	void mount(std::string driveid, std::string cartridgeid);
	void unmount(std::string cartridgeid);
	void format(std::string cartridgeid);
	void check(std::string cartridgeid);
};

extern OpenLTFSInventory *inventory;

#endif /* _INVENTORY_H */
