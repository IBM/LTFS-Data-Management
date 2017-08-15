#pragma once

template<typename ... Args> class ThreadPool
{
private:
    std::mutex enqueue_mtx; // seems to be necessary

	std::mutex mtx_main;
	std::condition_variable cond_main;
	std::condition_variable cond_init;
	std::mutex mtx_resp;
	std::condition_variable cond_resp;
	std::condition_variable cond_cont;
	std::condition_variable cond_fin;

	std::packaged_task<void()> task;
	std::atomic<int> started;
	std::atomic<int> occupied;
	std::atomic<bool> new_work;
	std::atomic<int> global_req_num;
	std::map<int, long> numJobs;

    const std::function<void(Args ... args)> func;
    int num_thrds;
	std::atomic<int> num_thrds_started;
	std::thread *new_thread;
	std::thread *last_thread;
    std::string name;


public:
    void threadfunc()
    {
        int req_num;
		std::thread *t = last_thread;

        pthread_setname_np(pthread_self(), name.c_str());

        std::packaged_task < void() > ltask;
        std::unique_lock < std::mutex > lock(mtx_main);
        started++;
        cond_init.notify_one();

        while (true) {
            cond_main.wait_for(lock, std::chrono::seconds(10));
            if ( new_work == false ) {
				lock.unlock();
				if ( t == nullptr ) {
					num_thrds_started--;
					cond_main.notify_all();
					return;
				}
                num_thrds_started--;
                cond_main.notify_all();
				t->join();
				delete(t);
                return;
            }
            new_work = false;
            occupied++;
            req_num = global_req_num;
            numJobs[req_num]++;

            ltask = std::move(task);

            lock.unlock();

            std::unique_lock < std::mutex > lock2(mtx_resp);
            cond_resp.notify_one();
            lock2.unlock();

            ltask();
            ltask.reset();

            lock.lock();

            occupied--;
            numJobs[req_num]--;
            cond_fin.notify_all();
            if (occupied == num_thrds - 1)
                cond_cont.notify_one();
        }
    }

    ThreadPool(std::function<void(Args ... args)> func_, int num_thrds_,
            std::string name_) :
	func(func_), num_thrds(num_thrds_), num_thrds_started(0),
		new_thread(nullptr), last_thread(nullptr),  name(name_)

    {
        started = 0;
        occupied = 0;
        new_work = false;
    }

    static void execute(std::function<void(Args ... args)> func, Args ... args)
    {
        func(args ...);
    }

    void enqueue(int req_num, Args ... args)
    {
		std::vector<std::thread>::iterator it;

        std::lock_guard < std::mutex > elock(enqueue_mtx);
        std::unique_lock < std::mutex > lock(mtx_main);
        new_work = true;
        if (occupied == num_thrds)
            cond_cont.wait(lock);


        if ( num_thrds_started == occupied ) {
            new_thread = new std::thread(&ThreadPool::threadfunc, this);
            num_thrds_started++;
            cond_init.wait(lock);
			last_thread = new_thread;
			new_thread = nullptr;
        }

        global_req_num = req_num;

        std::packaged_task < void() > task_(std::bind(execute, func, args ...));
        task = std::move(task_);

        std::unique_lock < std::mutex > lock2(mtx_resp);
        cond_main.notify_one();
        lock.unlock();
        cond_resp.wait(lock2);
    }

    void waitCompletion(int req_num)

    {
        std::unique_lock < std::mutex > lock(mtx_main);
        cond_fin.wait(lock,
                [this, req_num] {return numJobs[req_num] == 0;});
        numJobs.erase(req_num);
    }

    void terminate()
    {
        std::unique_lock < std::mutex > lock(mtx_main);

		cond_main.wait(lock, [this] {return num_thrds_started == 0;});
		last_thread->join();
		delete(last_thread);
		last_thread = nullptr;

		return;
    }
};
