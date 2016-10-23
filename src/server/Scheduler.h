#ifndef _SCHEDULER_H
#define _SCHEDULER_H

class Scheduler

{
public:
	static std::mutex mtx;
	static std::condition_variable cond;
	Scheduler() {}
	~Scheduler() {};
	void run(long key);
};

#endif /* _SCHEDULER_H */
