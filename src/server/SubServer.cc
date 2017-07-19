#include "ServerIncludes.h"

void SubServer::waitThread(std::string label, std::shared_future<void> task, std::shared_future<void> prev_waiter)

{
	int countb;

	TRACE(Trace::always, label);

	try {
		task.get();
	}
	catch ( const std::exception& e ) {
		MSG(LTFSDMS0074E, e.what());
		TRACE(Trace::always, e.what());
		Server::forcedTerminate = true;
		Connector::forcedTerminate = true;
		kill(getpid(), SIGUSR1);
	}

	countb = --count;

	if ( countb < maxThreads ) {
		bcond.notify_one();
	}

	if ( ! countb ) {
		econd.notify_one();
	}

	if ( prev_waiter.valid() == true) {
		prev_waiter.get();
	}
}

void SubServer::waitAllRemaining()
{
	if ( prev_waiter.valid() == true ) {

		try {
			prev_waiter.get();
		}
		catch ( const std::exception& e ) {
			MSG(LTFSDMS0074E, e.what());
			TRACE(Trace::always, e.what());
			Server::forcedTerminate = true;
			Connector::forcedTerminate = true;
			kill(getpid(), SIGUSR1);
		}

		std::unique_lock<std::mutex> lock(emtx);
		econd.wait(lock, [this]{return count == 0;});
	}
}
