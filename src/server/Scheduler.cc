#include "limits.h"

#include <string>
#include <sstream>
#include <mutex>
#include <condition_variable>
#include <thread>
#include <typeinfo>

#include <sqlite3.h>
#include "src/common/comm/ltfsdm.pb.h"

#include "src/common/tracing/Trace.h"
#include "src/common/util/util.h"
#include "src/connector/Connector.h"
#include "SubServer.h"
#include "Server.h"
#include "DataBase.h"
#include "Scheduler.h"

std::mutex Scheduler::mtx;
std::condition_variable Scheduler::cond;

void preMigrate(std::string fileName, std::string tapeId)

{
	// getHandle
	// requestRights (exclusive)
	// copy data (dm_reas_invis()))
	// add attribute
	// create managed region
	// release right
}

void stub(std::string fileName)

{
	// getHandle
	// requestRights (exclusive)
	// punchHole
	// release right
}

void migrationStep(int reqNum, int colGrp, std::string tapeId, int fromState, int toState)

{
	sqlite3_stmt *stmt;
	std::stringstream ssql;
	int rc, rc2;
    int group_end = 0;
    int group_begin = -1;
	long previous;
	long current;

	ssql << "SELECT ROWID, * FROM JOB_QUEUE WHERE REQ_NUM=" << reqNum << " AND COLOC_GRP=" << colGrp << " AND FILE_STATE=" << fromState;

	sqlite3_statement::prepare(ssql.str(), &stmt);

	std::cout << "Processing files for request " << reqNum << " and colocation group " << colGrp << std::endl;

	previous = time(NULL);

	while ( (rc = sqlite3_statement::step(stmt) ) ) {
		if ( rc != SQLITE_ROW && rc != SQLITE_DONE )
			break;

		if ( rc == SQLITE_ROW ) {
			const char *cstr = reinterpret_cast<const char*>(sqlite3_column_text (stmt, 2));
			std::cout << "Processing of " << (cstr ? std::string(cstr) : std::string("")) << " to " << tapeId << std::endl;
			usleep(random() % 100000);

			group_end = sqlite3_column_int(stmt, 0);
			if ( group_begin == -1 )
				group_begin = group_end;
			current = time(NULL);
			if ( current - previous < 1 )
				continue;
			previous = current;
		}

		ssql.str("");
		ssql.clear();

		ssql << "UPDATE JOB_QUEUE SET FILE_STATE = " <<  toState;
		ssql << " WHERE COLOC_GRP = " << colGrp << " AND (ROWID BETWEEN " << group_begin << " AND " << group_end << ")";

		sqlite3_stmt *stmt2;

		sqlite3_statement::prepare(ssql.str(), &stmt2);
		rc2 = sqlite3_statement::step(stmt2);
		sqlite3_statement::checkRcAndFinalize(stmt2, rc2, SQLITE_DONE);

		if ( rc == SQLITE_DONE )
			break;
	}

	sqlite3_statement::checkRcAndFinalize(stmt, rc, SQLITE_DONE);
}

void migration(int reqNum, int tgtState, int colGrp, std::string tapeId)

{
	TRACE(Trace::much, __PRETTY_FUNCTION__);

	sqlite3_stmt *stmt;
	std::stringstream ssql;
	int rc;

	migrationStep(reqNum, colGrp, tapeId,  DataBase::RESIDENT, DataBase::PREMIGRATED);

	if ( tgtState != LTFSDmProtocol::LTFSDmMigRequest::PREMIGRATED )
		migrationStep(reqNum, colGrp, tapeId,  DataBase::PREMIGRATED, DataBase::MIGRATED);

	std::unique_lock<std::mutex> lock(Scheduler::mtx);

	ssql << "UPDATE REQUEST_QUEUE SET STATE=" << DataBase::REQ_COMPLETED;
	ssql << " WHERE REQ_NUM=" << reqNum << " AND COLOC_GRP=" << colGrp << ";";

	sqlite3_statement::prepare(ssql.str(), &stmt);

	rc = sqlite3_statement::step(stmt);

	sqlite3_statement::checkRcAndFinalize(stmt, rc, SQLITE_DONE);

	ssql.str("");
	ssql.clear();
	ssql << "UPDATE TAPE_LIST SET STATE=" << DataBase::TAPE_FREE;
	ssql << " WHERE TAPE_ID='" << tapeId << "';";

	sqlite3_statement::prepare(ssql.str(), &stmt);

	rc = sqlite3_statement::step(stmt);

	sqlite3_statement::checkRcAndFinalize(stmt, rc, SQLITE_DONE);

	Scheduler::cond.notify_one();
}


void selrecall(int reqNum, int tgtState, std::string tapeID) {}

void Scheduler::run(long key)

{
	TRACE(Trace::much, __PRETTY_FUNCTION__);
	sqlite3_stmt *stmt;
	std::stringstream ssql;
	std::unique_lock<std::mutex> lock(mtx);
	SubServer subs;
	int rc;

	while (true) {
		cond.wait(lock);
		if(terminate == true) {
			break;
		}

		ssql << "SELECT * FROM REQUEST_QUEUE WHERE STATE=" << DataBase::REQ_NEW;
		ssql << " AND TAPE_ID IN (SELECT TAPE_ID FROM TAPE_LIST WHERE STATE=";
		ssql << DataBase::TAPE_FREE << ") ORDER BY OPERATION,REQ_NUM;";

		sqlite3_statement::prepare(ssql.str(), &stmt);

		if ( (rc = sqlite3_statement::step(stmt)) == SQLITE_ROW ) {
			std::stringstream ssql2;
			sqlite3_stmt *stmt2;

			const char *tape_id = reinterpret_cast<const char*>(sqlite3_column_text (stmt, 4));

			if (tape_id == NULL) {
				MSG(LTFSDMS0020E);
				continue;
			}

			std::string tapeId = std::string(tape_id);

			ssql2 << "UPDATE TAPE_LIST SET STATE=" << DataBase::TAPE_INUSE;
			ssql2 << " WHERE TAPE_ID='" << tapeId << "';";

			sqlite3_statement::prepare(ssql2.str(), &stmt2);

			rc = sqlite3_statement::step(stmt2);

			sqlite3_statement::checkRcAndFinalize(stmt2, rc, SQLITE_DONE);

			int reqNum = sqlite3_column_int(stmt, 1);
			int tgtState = sqlite3_column_int(stmt, 2);
			int colGrp = sqlite3_column_int(stmt, 3);
			std::stringstream thrdinfo;

			switch (sqlite3_column_int(stmt, 0)) {
				case DataBase::MIGRATION:
					thrdinfo << "Migration(" << reqNum << "," << colGrp << ")";
					subs.enqueue(thrdinfo.str(), migration, reqNum, tgtState, colGrp, tapeId);
					break;
				case DataBase::SELRECALL:
					thrdinfo << "S.Recall(" << reqNum << ")";
					subs.enqueue(thrdinfo.str(), selrecall, reqNum, tgtState, tapeId);
					break;
				default:
					TRACE(Trace::error, sqlite3_column_int(stmt, 0));
			}
		}

		sqlite3_statement::checkRcAndFinalize(stmt, rc, SQLITE_DONE);
	}
}
