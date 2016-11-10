#include <sys/types.h>
#include <sys/xattr.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <string>
#include <sstream>
#include <mutex>
#include <condition_variable>
#include <thread>
#include <typeinfo>
#include <map>

#include <sqlite3.h>
#include "src/common/comm/ltfsdm.pb.h"

#include "src/common/tracing/Trace.h"
#include "src/common/util/util.h"
#include "src/connector/Connector.h"
#include "SubServer.h"
#include "Server.h"
#include "DataBase.h"
#include "FileOperation.h"
#include "Migration.h"
#include "SelRecall.h"
#include "Scheduler.h"

std::mutex Scheduler::mtx;
std::condition_variable Scheduler::cond;
std::mutex Scheduler::updmtx;
std::condition_variable Scheduler::updcond;
std::atomic<int> Scheduler::updReq(Const::UNSET);
std::map<std::string, std::atomic<bool>> Scheduler::suspend_map;


std::string Scheduler::getTapeName(std::string fileName, std::string tapeId)

{
	FsObj diskFile(fileName);
	std::stringstream tapeName;

	tapeName << Const::LTFS_PATH
			 << "/" << tapeId << "/"
			 << Const::LTFS_NAME << "."
			 << diskFile.getFsId() << "."
			 << diskFile.getIGen() << "."
			 << diskFile.getINode();

	return tapeName.str();
}


void Scheduler::run(long key)

{
	TRACE(Trace::much, __PRETTY_FUNCTION__);
	sqlite3_stmt *stmt;
	std::stringstream ssql;
	std::unique_lock<std::mutex> lock(mtx);
	SubServer subs;
	int rc;

	suspend_map["DV1480L6"] = false;
	suspend_map["DV1481L6"] = false;

	while (true) {
		cond.wait(lock);
		if(terminate == true) {
			break;
		}

		ssql.str("");
		ssql.clear();
		ssql << "SELECT OPERATION, REQ_NUM, TARGET_STATE, COLOC_GRP, TAPE_ID";
		ssql << " FROM REQUEST_QUEUE WHERE STATE=" << DataBase::REQ_NEW;
		ssql << " ORDER BY OPERATION,REQ_NUM;";

		sqlite3_statement::prepare(ssql.str(), &stmt);

		while ( (rc = sqlite3_statement::step(stmt)) == SQLITE_ROW ) {
			sqlite3_stmt *stmt2;

			const char *tape_id = reinterpret_cast<const char*>(sqlite3_column_text (stmt, 4));
			if (tape_id == NULL) {
				MSG(LTFSDMS0020E);
				continue;
			}
			std::string tapeId = std::string(tape_id);

			ssql.str("");
			ssql.clear();
			ssql << "SELECT STATE FROM TAPE_LIST WHERE TAPE_ID='" << tapeId << "';";

			sqlite3_statement::prepare(ssql.str(), &stmt2);
			while ( (rc = sqlite3_statement::step(stmt2)) == SQLITE_ROW ) {
				if ( sqlite3_column_int(stmt2, 0) != DataBase::TAPE_FREE ) {
					if ( sqlite3_column_int(stmt, 0) == DataBase::SELRECALL ) {
						suspend_map[tapeId] = true;
					}
					continue;
				}

				sqlite3_stmt *stmt3;

				ssql.str("");
				ssql.clear();
				ssql << "UPDATE TAPE_LIST SET STATE=" << DataBase::TAPE_INUSE;
				ssql << " WHERE TAPE_ID='" << tapeId << "';";
				sqlite3_statement::prepare(ssql.str(), &stmt3);
				rc = sqlite3_statement::step(stmt3);
				sqlite3_statement::checkRcAndFinalize(stmt3, rc, SQLITE_DONE);

				int reqNum = sqlite3_column_int(stmt, 1);
				int tgtState = sqlite3_column_int(stmt, 2);
				int colGrp = sqlite3_column_int(stmt, 3);


				std::stringstream thrdinfo;

				switch (sqlite3_column_int(stmt, 0)) {
					case DataBase::MIGRATION:
						ssql.str("");
						ssql.clear();
						ssql << "UPDATE REQUEST_QUEUE SET STATE=" << DataBase::REQ_INPROGRESS;
						ssql << " WHERE REQ_NUM=" << reqNum << " AND COLOC_GRP=" << colGrp << ";";
						sqlite3_statement::prepare(ssql.str(), &stmt3);
						rc = sqlite3_statement::step(stmt3);
						sqlite3_statement::checkRcAndFinalize(stmt3, rc, SQLITE_DONE);

						thrdinfo << "Migration(" << reqNum << "," << colGrp << ")";
						subs.enqueue(thrdinfo.str(), Migration::execRequest, reqNum, tgtState, colGrp, tapeId);
						break;
					case DataBase::SELRECALL:
						ssql.str("");
						ssql.clear();
						ssql << "UPDATE REQUEST_QUEUE SET STATE=" << DataBase::REQ_INPROGRESS;
						ssql << " WHERE REQ_NUM=" << reqNum << " AND TAPE_ID='" << tapeId << "';";
						sqlite3_statement::prepare(ssql.str(), &stmt3);
						rc = sqlite3_statement::step(stmt3);
						sqlite3_statement::checkRcAndFinalize(stmt3, rc, SQLITE_DONE);

						thrdinfo << "S.Recall(" << reqNum << ")";
						subs.enqueue(thrdinfo.str(), SelRecall::execRequest, reqNum, tgtState, tapeId);
						break;
					default:
						TRACE(Trace::error, sqlite3_column_int(stmt, 0));
				}
			}
			sqlite3_statement::checkRcAndFinalize(stmt2, rc, SQLITE_DONE);
		}
		sqlite3_statement::checkRcAndFinalize(stmt, rc, SQLITE_DONE);
	}
	subs.waitAllRemaining();
}
