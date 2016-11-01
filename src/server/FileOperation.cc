#include <string>
#include <sstream>
#include <mutex>
#include <condition_variable>

#include <sqlite3.h>
#include "src/common/comm/ltfsdm.pb.h"

#include "src/common/util/util.h"
#include "src/common/messages/Message.h"
#include "src/common/tracing/Trace.h"
#include "src/common/errors/errors.h"
#include "src/common/const/Const.h"

#include "src/connector/Connector.h"

#include "DataBase.h"
#include "Scheduler.h"
#include "FileOperation.h"

bool FileOperation::queryResult(long reqNumber, long *resident, long *premigrated, long *migrated)

{
	int rc;
	std::stringstream ssql;
	sqlite3_stmt *stmt;
	bool done = true;
	std::unique_lock<std::mutex> lock(Scheduler::updmtx);

	ssql << "SELECT STATE FROM REQUEST_QUEUE WHERE REQ_NUM=" << reqNumber;

	sqlite3_statement::prepare(ssql.str(), &stmt);

	while ( (rc = sqlite3_statement::step(stmt)) == SQLITE_ROW ) {
		if ( sqlite3_column_int(stmt, 0) != DataBase::REQ_COMPLETED ) {
			done = false;
		}
	}
	sqlite3_statement::checkRcAndFinalize(stmt, rc, SQLITE_DONE);

	if ( done == false )
		Scheduler::updcond.wait(lock);

	ssql.str("");
	ssql.clear();

	ssql << "SELECT FILE_STATE, COUNT(*) FROM JOB_QUEUE WHERE REQ_NUM=" << reqNumber << " GROUP BY FILE_STATE";

	sqlite3_statement::prepare(ssql.str(), &stmt);

	*resident = 0;
	*premigrated = 0;
	*migrated = 0;

	while ( (rc = sqlite3_statement::step(stmt)) == SQLITE_ROW ) {
		switch ( sqlite3_column_int(stmt, 0) ) {
			case FsObj::RESIDENT:
				*resident = sqlite3_column_int(stmt, 1);
				break;
			case FsObj::PREMIGRATED:
				*premigrated = sqlite3_column_int(stmt, 1);
				break;
			case FsObj::MIGRATED:
				*migrated = sqlite3_column_int(stmt, 1);
				break;
			default:
				TRACE(Trace::error, sqlite3_column_int(stmt, 0));
				throw(Error::LTFSDM_GENERAL_ERROR);
		}
	}

	sqlite3_statement::checkRcAndFinalize(stmt, rc, SQLITE_DONE);

	if ( done ) {
		ssql.str("");
		ssql.clear();

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
