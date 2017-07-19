#include "ServerIncludes.h"

bool FileOperation::queryResult(long reqNumber, long *resident,
								long *premigrated, long *migrated,
								long *failed)

{
	int rc;
	std::stringstream ssql;
	sqlite3_stmt *stmt;
	bool done;
	long starttime = time(NULL);

	do {
		ssql.str("");
		ssql.clear();
		done = true;

		std::unique_lock<std::mutex> lock(Scheduler::updmtx);

		ssql << "SELECT STATE FROM REQUEST_QUEUE WHERE REQ_NUM=" << reqNumber;

		sqlite3_statement::prepare(ssql.str(), &stmt);

		while ( (rc = sqlite3_statement::step(stmt)) == SQLITE_ROW ) {
			if ( sqlite3_column_int(stmt, 0) != DataBase::REQ_COMPLETED ) {
				done = false;
			}
		}
		sqlite3_statement::checkRcAndFinalize(stmt, rc, SQLITE_DONE);

		if ( Server::finishTerminate == true )
			done = true;

		if ( done == false ) {
			TRACE(Trace::full, reqNumber);
			TRACE(Trace::full, (bool) Scheduler::updReq[reqNumber]);
			Scheduler::updcond.wait(lock, [reqNumber]{return ((Server::finishTerminate == true) || (Scheduler::updReq[reqNumber] == true));});
			Scheduler::updReq[reqNumber] = false;
		}
	} while(!done && time(NULL) - starttime < 10);

	mrStatus.get(reqNumber, resident, premigrated, migrated, failed);

	if ( done ) {
		mrStatus.remove(reqNumber);

		ssql.str("");
		ssql.clear();

		{
			std::lock_guard<std::mutex> updlock(Scheduler::updmtx);
			Scheduler::updReq.erase(Scheduler::updReq.find(reqNumber));
		}

		ssql << "DELETE FROM JOB_QUEUE WHERE REQ_NUM=" << reqNumber;
		sqlite3_statement::prepare(ssql.str(), &stmt);
		rc = sqlite3_statement::step(stmt);
		sqlite3_statement::checkRcAndFinalize(stmt, rc, SQLITE_DONE);

		ssql.str("");
		ssql.clear();

		ssql << "DELETE FROM REQUEST_QUEUE WHERE REQ_NUM=" << reqNumber;
		sqlite3_statement::prepare(ssql.str(), &stmt);
		rc = sqlite3_statement::step(stmt);
		sqlite3_statement::checkRcAndFinalize(stmt, rc, SQLITE_DONE);
	}

	return done;
}
