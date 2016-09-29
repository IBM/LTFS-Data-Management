#include <atomic>
#include <condition_variable>
#include <mutex>
#include <thread>


#include "SubServer.h"

void SubServer::wait_single(std::thread *thrd, std::thread *thrdprev)

{
	int countb;

	thrd->join();
	delete(thrd);
	countb = --count;

	if ( countb < maxThreads ) {
		bcond.notify_one();
	}

	if ( ! countb ) {
		econd.notify_one();
	}

	if ( thrdprev ) {
		thrdprev->join();
		delete(thrdprev);
	}
}
