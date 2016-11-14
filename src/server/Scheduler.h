#ifndef _SCHEDULER_H
#define _SCHEDULER_H

typedef struct {
	int reqNum;
	int colGrp;
	std::string tapeId;
} reqIdent_t;

class Scheduler

{
public:
	static std::mutex mtx;
	static std::condition_variable cond;

	static std::mutex updmtx;
	static std::condition_variable updcond;
	static std::atomic<int> updReq;

	static std::mutex reqmtx;
	static std::condition_variable reqcond;
	static reqIdent_t reqIdent;

	static std::map<std::string, std::atomic<bool>> suspend_map;
	static std::string getTapeName(std::string fileName, std::string tapeId);
	Scheduler() {}
	~Scheduler() {};
	void run(long key);
};

#endif /* _SCHEDULER_H */
