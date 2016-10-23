#include "limits.h"

#include <string>
#include <sstream>
#include <mutex>
#include <condition_variable>
#include <thread>
#include <typeinfo>

#include <sqlite3.h>

#include "src/common/tracing/Trace.h"
#include "src/common/util/util.h"
#include "src/connector/Connector.h"
#include "SubServer.h"
#include "Server.h"
#include "DataBase.h"
#include "Scheduler.h"

std::mutex Scheduler::mtx;
std::condition_variable Scheduler::cond;

void migration(int reqNum, int tgtState, int colGrp, std::string tapeId)

{
	TRACE(Trace::much, __PRETTY_FUNCTION__);
	sqlite3_stmt *stmt;
	std::stringstream ssql;
	int rc;

	ssql << "SELECT * FROM JOB_QUEUE WHERE REQ_NUM=" << reqNum << " AND COLOC_GRP=" << colGrp << ";";

	DataBase::prepare(ssql.str(), &stmt);

	std::cout << "Migrating files for request " << reqNum << " and colocation group " << colGrp << std::endl;

	while ( (rc = DataBase::step(stmt)) == SQLITE_ROW ) {
		const char *cstr = reinterpret_cast<const char*>(sqlite3_column_text (stmt, 1));
		std::cout << "Migration of " << (cstr ? std::string(cstr) : std::string("")) << " to " << tapeId << std::endl;
		usleep(random() % 1000000);
	}

	DataBase::checkRcAndFinalize(stmt, rc, SQLITE_DONE);

	std::unique_lock<std::mutex> lock(Scheduler::mtx);

	ssql.str("");
	ssql.clear();
	ssql << "UPDATE REQUEST_QUEUE SET STATE=" << DataBase::REQ_COMPLETED;
	ssql << " WHERE REQ_NUM=" << reqNum << " AND COLOC_GRP=" << colGrp << ";";

	DataBase::prepare(ssql.str(), &stmt);

	rc = DataBase::step(stmt);

	DataBase::checkRcAndFinalize(stmt, rc, SQLITE_DONE);

	ssql.str("");
	ssql.clear();
	ssql << "UPDATE TAPE_LIST SET STATE=" << DataBase::TAPE_FREE;
	ssql << " WHERE TAPE_ID='" << tapeId << "';";

	DataBase::prepare(ssql.str(), &stmt);

	rc = DataBase::step(stmt);

	DataBase::checkRcAndFinalize(stmt, rc, SQLITE_DONE);

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

		DataBase::prepare(ssql.str(), &stmt);

		if ( (rc = DataBase::step(stmt)) == SQLITE_ROW ) {
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

			DataBase::prepare(ssql2.str(), &stmt2);

			rc = DataBase::step(stmt2);

			DataBase::checkRcAndFinalize(stmt2, rc, SQLITE_DONE);

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

		DataBase::checkRcAndFinalize(stmt, rc, SQLITE_DONE);
	}
}
