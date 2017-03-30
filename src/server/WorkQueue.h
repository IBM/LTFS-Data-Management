#ifndef _WORKQUEUE_H
#define _WORKQUEUE_H


template<typename... Args> class WorkQueue {
private:
	struct wq_data_t {
		std::mutex mtx_add;
		std::condition_variable cond_add;
		std::condition_variable cond_init;
		std::mutex mtx_resp;
		std::condition_variable cond_resp;
		std::condition_variable cond_cont;

		std::packaged_task<void ()> task;
		std::atomic<bool> terminate;
		std::atomic<int> started;
		std::atomic<int> occupied;
	} wq_data;

	const std::function<void (Args... args)> func;
	int num_thrds;
	std::vector<std::thread> threads;
public:
	static void threadfunc(int i, int num_thrds, wq_data_t *wq_data)
	{
		std::packaged_task<void ()> ltask;
		std::unique_lock<std::mutex> lock(wq_data->mtx_add);
		wq_data->started++;
		wq_data->cond_init.notify_one();

		while ( true ) {
			wq_data->cond_add.wait(lock);
			wq_data->occupied++;
			if ( wq_data->terminate == true )
				return;

			ltask = std::move(wq_data->task);

			lock.unlock();

			std::unique_lock<std::mutex> lock2(wq_data->mtx_resp);
			wq_data->cond_resp.notify_one();
			lock2.unlock();

			ltask();

			lock.lock();

			if ( wq_data->terminate == true )
				return;

			wq_data->occupied--;
			if ( wq_data->occupied == num_thrds - 1)
				wq_data->cond_cont.notify_one();
		}
	}

	WorkQueue(std::function<void (Args... args)> func_, int num_thrds_) : func(func_), num_thrds(num_thrds_)
	{
		wq_data.terminate = false;
		wq_data.started = 0;
		wq_data.occupied = 0;

		for (int i=0; i<num_thrds; i++)
			threads.push_back(std::thread(&threadfunc, i, num_thrds, &wq_data));

		std::unique_lock<std::mutex> lock(wq_data.mtx_add);
		wq_data.cond_init.wait(lock,[this]{return wq_data.started == num_thrds;});
	}

	static void execute(std::function<void (Args... args)> func, Args... args)
	{
		func(args ...);
	}

	void enqueue(Args... args)
	{
		std::unique_lock<std::mutex> lock(wq_data.mtx_add);
		if (wq_data.occupied == num_thrds)
			wq_data.cond_cont.wait(lock);

		std::packaged_task<void ()>  task_(std::bind(execute, func, args ...));
		wq_data.task = std::move(task_);

		std::unique_lock<std::mutex> lock2(wq_data.mtx_resp);
		wq_data.cond_add.notify_one();
		lock.unlock();
		wq_data.cond_resp.wait(lock2);
	}

	void terminate()
	{
		std::unique_lock<std::mutex> lock(wq_data.mtx_add);
		wq_data.terminate = true;
		wq_data.cond_add.notify_all();
		lock.unlock();
		for (int i=0; i<num_thrds; i++) {
			threads[i].join();
		}
	}
};

#endif /* _WORKQUEUE_H */
