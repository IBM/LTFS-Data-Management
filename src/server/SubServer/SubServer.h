#ifndef _SUBSERVER_H
#define _SUBSERVER_H

class SubServer {
private:
	std::atomic<int> count;
	std::atomic<int> maxThreads;
	std::mutex emtx;
	std::condition_variable econd;
	std::mutex bmtx;
	std::condition_variable bcond;
	std::thread *thrdprev;
	void waitThread(std::thread *thrd, std::thread *thrdprev);
public:
	SubServer() : count(0), maxThreads(INT_MAX), thrdprev(nullptr) {}
	SubServer(int _maxThreads) : count(0), maxThreads(_maxThreads) {}

	void waitAllRemaining();

	template< typename Function, typename... Args >
	void enqueue(Function&& f, Args... args)
	{
		int countb;
		std::unique_lock<std::mutex> lock(bmtx);

		countb = ++count;

		if ( countb >= maxThreads ) {
			bcond.wait(lock);
		}

		std::thread *thrd1 = new std::thread(f, args ...);
		TRACE(Trace::much, thrd1->get_id());
		std::thread *thrd2 = new std::thread(&SubServer::waitThread, this, thrd1, thrdprev);
		TRACE(Trace::much, thrd2->get_id());
		thrdprev = thrd2;
	}
};

#endif /*_SUBSERVER_H */
