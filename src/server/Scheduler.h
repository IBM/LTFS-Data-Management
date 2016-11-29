#ifndef _SCHEDULER_H
#define _SCHEDULER_H

class Scheduler

{
public:
	static std::mutex mtx;
	static std::condition_variable cond;
	static std::mutex updmtx;
	static std::condition_variable updcond;
	static std::atomic<int> updReq;
	static std::map<std::string, std::atomic<bool>> suspend_map;
	static std::string getTapeName(std::string fileName, std::string tapeId);
	static std::string getTapeName(unsigned long long fsid, unsigned int igen,
								   unsigned long long ino, std::string tapeId);
	static long getStartBlock(std::string tapeName);
	Scheduler() {}
	~Scheduler() {};
	void run(long key);
};

#endif /* _SCHEDULER_H */
