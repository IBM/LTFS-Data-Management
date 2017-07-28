#pragma once

class SubServer
{
private:
	std::atomic<int> count;
	std::atomic<int> maxThreads;
	std::mutex emtx;
	std::condition_variable econd;
	std::mutex bmtx;
	std::condition_variable bcond;
	std::shared_future<void> prev_waiter;
	void waitThread(std::string label, std::shared_future<void> thrd,
			std::shared_future<void> prev_waiter);
public:
	SubServer() :
			count(0), maxThreads(INT_MAX)
	{
	}
	SubServer(int _maxThreads) :
			count(0), maxThreads(_maxThreads)
	{
	}

	void waitAllRemaining();

	template<typename Function, typename ... Args>
	void enqueue(std::string label, Function&& f, Args ... args)
	{
		int countb;
		char threadName[64];
		std::unique_lock < std::mutex > lock(bmtx);

		countb = ++count;

		if (countb >= maxThreads) {
			bcond.wait(lock);
		}

		memset(threadName, 0, 64);
		pthread_getname_np(pthread_self(), threadName, 63);
		pthread_setname_np(pthread_self(), label.c_str());
		std::shared_future<void> task = std::async(std::launch::async, f,
				args ...);
		pthread_setname_np(pthread_self(), (std::string("w:") + label).c_str());
		std::shared_future<void> waiter = std::async(std::launch::async,
				&SubServer::waitThread, this, label, task, prev_waiter);
		pthread_setname_np(pthread_self(), threadName);
		prev_waiter = waiter;
	}
};
