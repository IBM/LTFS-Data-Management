#include <sys/types.h>
#include <sys/xattr.h>
#include <sys/stat.h>
#include <sys/resource.h>
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
#include "TransRecall.h"
#include "Scheduler.h"

std::mutex Scheduler::mtx;
std::condition_variable Scheduler::cond;
std::mutex Scheduler::updmtx;
std::condition_variable Scheduler::updcond;
std::map<int, std::atomic<bool>> Scheduler::updReq;
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


std::string Scheduler::getTapeName(unsigned long long fsid, unsigned int igen,
								   unsigned long long ino, std::string tapeId)

{
	std::stringstream tapeName;

	tapeName << Const::LTFS_PATH
			 << "/" << tapeId << "/"
			 << Const::LTFS_NAME << "."
			 << fsid << "."
			 << igen << "."
			 << ino;

	return tapeName.str();
}


long Scheduler::getStartBlock(std::string tapeName)

{
	long size;
	char startBlockStr[32];
	long startBlock;

	memset(startBlockStr, 0, sizeof(startBlockStr));

	size = getxattr(tapeName.c_str(), Const::LTFS_START_BLOCK.c_str(), startBlockStr, sizeof(startBlockStr));

	if ( size == -1 ) {
		TRACE(Trace::error, tapeName.c_str());
		TRACE(Trace::error, errno);
		return Const::UNSET;
	}

	startBlock = strtol(startBlockStr, NULL, 0);

	if ( startBlock == LONG_MIN || startBlock == LONG_MAX )
		return Const::UNSET;
	else
		return startBlock;
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
			lock.unlock();
			break;
		}

		ssql.str("");
		ssql.clear();
		ssql << "SELECT OPERATION, REQ_NUM, TARGET_STATE, NUM_REPL,"
			 << " REPL_NUM, COLOC_GRP, TAPE_ID"
			 << " FROM REQUEST_QUEUE WHERE STATE=" << DataBase::REQ_NEW
			 << " ORDER BY OPERATION,REQ_NUM;";

		sqlite3_statement::prepare(ssql.str(), &stmt);

		while ( (rc = sqlite3_statement::step(stmt)) == SQLITE_ROW ) {
			sqlite3_stmt *stmt2;

			const char *tape_id = reinterpret_cast<const char*>(sqlite3_column_text (stmt, 6));
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
					if ( sqlite3_column_int(stmt, 0) == DataBase::SELRECALL ||
						 sqlite3_column_int(stmt, 0) == DataBase::TRARECALL) {
						suspend_map[tapeId] = true;
					}
					continue;
				}

				sqlite3_stmt *stmt3;

				ssql.str("");
				ssql.clear();
				ssql << "UPDATE TAPE_LIST SET STATE=" << DataBase::TAPE_INUSE
					 << " WHERE TAPE_ID='" << tapeId << "';";
				sqlite3_statement::prepare(ssql.str(), &stmt3);
				rc = sqlite3_statement::step(stmt3);
				sqlite3_statement::checkRcAndFinalize(stmt3, rc, SQLITE_DONE);

				int reqNum = sqlite3_column_int(stmt, 1);
				int tgtState = sqlite3_column_int(stmt, 2);
				int numRepl = sqlite3_column_int(stmt, 3);
				int replNum = sqlite3_column_int(stmt, 4);
				int colGrp = sqlite3_column_int(stmt, 5);

				TRACE(Trace::little, reqNum);
				TRACE(Trace::little, tgtState);
				TRACE(Trace::little, numRepl);
				TRACE(Trace::little, replNum);
				TRACE(Trace::little, colGrp);
				TRACE(Trace::little, tape_id);


				std::stringstream thrdinfo;

				switch (sqlite3_column_int(stmt, 0)) {
					case DataBase::MIGRATION:
						ssql.str("");
						ssql.clear();
						ssql << "UPDATE REQUEST_QUEUE SET STATE=" << DataBase::REQ_INPROGRESS
							 << " WHERE REQ_NUM=" << reqNum
							 << " AND REPL_NUM=" << replNum
							 << " AND COLOC_GRP=" << colGrp << ";";
						sqlite3_statement::prepare(ssql.str(), &stmt3);
						rc = sqlite3_statement::step(stmt3);
						sqlite3_statement::checkRcAndFinalize(stmt3, rc, SQLITE_DONE);

						thrdinfo << "Mig(" << reqNum << "," << replNum << "," << colGrp << ")";
						subs.enqueue(thrdinfo.str(), Migration::execRequest, reqNum, tgtState, numRepl, replNum, colGrp, tapeId, true /* needsTape */);
						break;
					case DataBase::SELRECALL:
						ssql.str("");
						ssql.clear();
						ssql << "UPDATE REQUEST_QUEUE SET STATE=" << DataBase::REQ_INPROGRESS
							 << " WHERE REQ_NUM=" << reqNum
							 << " AND TAPE_ID='" << tapeId << "';";
						sqlite3_statement::prepare(ssql.str(), &stmt3);
						rc = sqlite3_statement::step(stmt3);
						sqlite3_statement::checkRcAndFinalize(stmt3, rc, SQLITE_DONE);

						thrdinfo << "SelRec(" << reqNum << ")";
						subs.enqueue(thrdinfo.str(), SelRecall::execRequest, reqNum, tgtState, tapeId, true /* needsTape */);
						break;
					case DataBase::TRARECALL:
						ssql.str("");
						ssql.clear();
						ssql << "UPDATE REQUEST_QUEUE SET STATE=" << DataBase::REQ_INPROGRESS
							 << " WHERE REQ_NUM=" << reqNum
							 << " AND TAPE_ID='" << tapeId << "';";
						sqlite3_statement::prepare(ssql.str(), &stmt3);
						rc = sqlite3_statement::step(stmt3);
						sqlite3_statement::checkRcAndFinalize(stmt3, rc, SQLITE_DONE);

						thrdinfo << "TraRec(" << reqNum << ")";
						subs.enqueue(thrdinfo.str(), TransRecall::execRequest, reqNum, tapeId);
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
