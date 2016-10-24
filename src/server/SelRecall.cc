#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include <iostream>
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
#include "FileOperation.h"
#include "Scheduler.h"
#include "SelRecall.h"

void SelRecall::addFileName(std::string fileName)

{
	int rc;
	struct stat statbuf;
	std::stringstream ssql;
	sqlite3_stmt *stmt;

	ssql << "INSERT INTO JOB_QUEUE (OPERATION, FILE_NAME, REQ_NUM, TARGET_STATE, COLOC_GRP, FILE_SIZE, FS_ID, I_GEN, I_NUM, MTIME, LAST_UPD, TAPE_ID, FILE_STATE, FAILED) ";
	ssql << "VALUES (" << DataBase::SELRECALL << ", ";            // OPERATION
	ssql << "'" << fileName << "', ";                             // FILE_NAME
	ssql << reqNumber << ", ";                                    // REQ_NUM
	ssql << targetState << ", ";                                  // MIGRATION_STATE
	ssql << "NULL" << ", ";                                       // COLOC_GRP

	try {
		FsObj fso(fileName);
		statbuf = fso.stat();

		if (!S_ISREG(statbuf.st_mode)) {
			MSG(LTFSDMS0018E, fileName.c_str());
			return;
		}

		ssql << statbuf.st_size << ", ";                          // FILE_SIZE

		ssql << fso.getFsId() << ", ";                            // FS_ID
		ssql << fso.getIGen() << ", ";                            // I_GEN
		ssql << fso.getINode() << ", ";                           // I_NUM
		ssql << statbuf.st_mtime << ", ";                         // MTIME
		ssql << time(NULL) << ", ";                               // LAST_UPD
		ssql << "'" << fso.getTapeId() << "', ";                  // TAPE_ID
		ssql << DataBase::MIGRATED << ", ";                       // FILE_STATE
		ssql << 0 << ");";                                        // FAILED
	}
	catch ( int error ) {
		MSG(LTFSDMS0017E, fileName.c_str());
		return;
	}

	sqlite3_statement::prepare(ssql.str(), &stmt);

	rc = sqlite3_statement::step(stmt);

	sqlite3_statement::checkRcAndFinalize(stmt, rc, SQLITE_DONE);

	return;
}

void SelRecall::addRequest()

{
	int rc;
	std::stringstream ssql;
	sqlite3_stmt *stmt;
	bool requestAdded = false;

	std::unique_lock<std::mutex> lock(Scheduler::mtx);

	ssql.str("");
	ssql.clear();

	ssql << "SELECT TAPE_ID FROM JOB_QUEUE WHERE REQ_NUM=" << reqNumber << " GROUP BY TAPE_ID";

	sqlite3_statement::prepare(ssql.str(), &stmt);

	while ( (rc = sqlite3_statement::step(stmt)) == SQLITE_ROW ) {
		std::stringstream ssql2;
		sqlite3_stmt *stmt2;

		const char *cstr = reinterpret_cast<const char*>(sqlite3_column_text (stmt, 0));

		ssql2 << "INSERT INTO REQUEST_QUEUE (OPERATION, REQ_NUM, TARGET_STATE, COLOC_GRP, TAPE_ID, TIME_ADDED, STATE) ";
		ssql2 << "VALUES (" << DataBase::SELRECALL << ", ";                                     // OPERATION
		ssql2 << reqNumber << ", ";                                                             // FILE_NAME
		ssql2 << targetState << ", ";                                                           // TARGET_STATE
		ssql2 << "NULL" << ", ";                                                                // COLOC_GRP
		ssql2 << "'" << (cstr ? std::string(cstr) : std::string("")) << "', ";                  // TAPE_ID
		ssql2 << time(NULL) << ", ";                                                            // TIME_ADDED
		ssql2 << DataBase::REQ_NEW << ");";                                                     // STATE

		sqlite3_statement::prepare(ssql2.str(), &stmt2);

		rc = sqlite3_statement::step(stmt2);

		sqlite3_statement::checkRcAndFinalize(stmt2, rc, SQLITE_DONE);
	}

	sqlite3_statement::checkRcAndFinalize(stmt, rc, SQLITE_DONE);

	if (requestAdded)
		Scheduler::cond.notify_one();
}
