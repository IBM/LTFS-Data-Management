#include "ServerIncludes.h"

std::string FileOperation::genInumString(std::list<unsigned long> inumList)

{
	std::stringstream inumss;

	bool first = true;
	for ( unsigned long inum : inumList ) {
		if ( first ) {
			inumss << inum;
			first = false;
		}
		else {
			inumss << ", " << inum;
		}
	}
	return inumss.str();
}

bool FileOperation::queryResult(long reqNumber, long *resident,
								long *premigrated, long *migrated,
								long *failed)

{
	SQLStatement stmt;
	int state;
	bool done;
	long starttime = time(NULL);

	do {
		done = true;

		std::unique_lock<std::mutex> lock(Scheduler::updmtx);

		stmt(FileOperation::REQUEST_STATE) % reqNumber;
		stmt.prepare();
		while ( stmt.step(&state) ) {
			if ( state != DataBase::REQ_COMPLETED ) {
				done = false;
				break;
			}
		}
		stmt.finalize();

		if ( Server::finishTerminate == true )
			done = true;

		if ( done == false ) {
			TRACE(Trace::full, reqNumber, (bool) Scheduler::updReq[reqNumber]);
			Scheduler::updcond.wait(lock, [reqNumber]{return ((Server::finishTerminate == true) || (Scheduler::updReq[reqNumber] == true));});
			Scheduler::updReq[reqNumber] = false;
		}
	} while(!done && time(NULL) - starttime < 10);

	mrStatus.get(reqNumber, resident, premigrated, migrated, failed);

	if ( done ) {
		mrStatus.remove(reqNumber);

		{
			std::lock_guard<std::mutex> updlock(Scheduler::updmtx);
			Scheduler::updReq.erase(Scheduler::updReq.find(reqNumber));
		}

		stmt(FileOperation::DELETE_JOBS) % reqNumber;
		stmt.doall();

		stmt(FileOperation::DELETE_REQUESTS) % reqNumber;
		stmt.doall();
	}

	return done;
}
