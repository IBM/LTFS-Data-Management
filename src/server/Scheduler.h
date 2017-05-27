#ifndef _SCHEDULER_H
#define _SCHEDULER_H

class Scheduler

{
private:
	DataBase::operation op;
	int reqNum;
	int tgtState;
	int numRepl;
	int replNum;
	std::string tapeId;
	std::string pool;
	bool poolResAvail();
	bool tapeResAvail();
	bool resAvail();
	SubServer subs;
public:
	static std::mutex mtx;
	static std::condition_variable cond;
	static std::mutex updmtx;
	static std::condition_variable updcond;
	static std::map<int, std::atomic<bool>> updReq;
	static std::map<std::string, std::atomic<bool>> suspend_map;
	static std::string getTapeName(std::string fileName, std::string tapeId);
	static std::string getTapeName(unsigned long long fsid, unsigned int igen,
								   unsigned long long ino, std::string tapeId);
	static long getStartBlock(std::string tapeName);
	static void mount(std::string driveid, std::string cartridgeid)
	{
		inventory->mount(driveid, cartridgeid);
	}
	static void unmount(std::string driveid, std::string cartridgeid)
	{
		inventory->unmount(driveid, cartridgeid);
	}
	Scheduler() {}
	~Scheduler() {};
	void run(long key);
};

#endif /* _SCHEDULER_H */
