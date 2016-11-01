#ifndef _SCHEDULER_H
#define _SCHEDULER_H

class Scheduler

{
public:
	static std::mutex mtx;
	static std::condition_variable cond;
	static std::mutex updmtx;
	static std::condition_variable updcond;
	static std::string getTapeName(std::string fileName, std::string tapeId);
	Scheduler() {}
	~Scheduler() {};
	void run(long key);
};

#endif /* _SCHEDULER_H */
