#include <atomic>
#include <condition_variable>
#include <mutex>
#include <thread>

#include "src/common/messages/Message.h"
#include "src/common/tracing/Trace.h"
#include "src/common/errors/errors.h"
#include "src/common/const/Const.h"

#include "SubServer.h"

void SubServer::waitThread(std::thread *thrd, std::thread *thrdprev)

{
	int countb;
	std::thread::id id = thrd->get_id();
	std::thread::id idprev;

	if ( thrdprev != nullptr )
		idprev = thrdprev->get_id();

	thrd->join();
	TRACE(Trace::much, id);
	delete(thrd);
	countb = --count;

	if ( countb < maxThreads ) {
		bcond.notify_one();
	}

	if ( ! countb ) {
		econd.notify_one();
	}

	if ( thrdprev != nullptr) {
		thrdprev->join();
		TRACE(Trace::much, idprev);
		delete(thrdprev);
	}
}

void SubServer::waitAllRemaining()
{
	if ( thrdprev ) {
		thrdprev->join();
		delete(thrdprev);

		std::unique_lock<std::mutex> lock(emtx);
		econd.wait(lock, [this]{return count == 0;});
	}
}
