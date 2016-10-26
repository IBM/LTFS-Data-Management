#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include <iostream>
#include <sstream>
#include <vector>

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
#include "Migration.h"

void Migration::addFileName(std::string fileName)

{
	int rc;
	struct stat statbuf;
	std::stringstream ssql;

	ssql << "INSERT INTO JOB_QUEUE (OPERATION, FILE_NAME, REQ_NUM, TARGET_STATE, COLOC_GRP, FILE_SIZE, FS_ID, I_GEN, I_NUM, MTIME, LAST_UPD, TAPE_ID, FILE_STATE, FAILED) ";
	ssql << "VALUES (" << DataBase::MIGRATION << ", ";            // OPERATION
	ssql << "'" << fileName << "', ";                             // FILE_NAME
	ssql << reqNumber << ", ";                                    // REQ_NUM
	ssql << targetState << ", ";                                  // MIGRATION_STATE
	ssql << jobnum % colFactor << ", ";                           // COLOC_GRP

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
		ssql << statbuf.st_mtime << ", ";                             // MTIME
		ssql << time(NULL) << ", ";                                   // LAST_UPD
		ssql << "NULL" << ", ";                                       // TAPE_ID
		ssql << fso.getMigState() << ", ";                            // FILE_STATE
		ssql << 0 << ");";                                            // FAILED
	}
	catch ( int error ) {
		MSG(LTFSDMS0017E, fileName.c_str());
		return;
	}


	sqlite3_statement::prepare(ssql.str(), &stmt);

	rc = sqlite3_step(stmt);

	sqlite3_statement::checkRcAndFinalize(stmt, rc, SQLITE_DONE);

	jobnum++;

	return;
}

std::vector<std::string> Migration::getTapes()

{
	int rc;
	std::string sql;
	sqlite3_stmt *stmt;

	sql = std::string("SELECT * FROM TAPE_LIST");

	sqlite3_statement::prepare(sql, &stmt);

	while ( (rc = sqlite3_statement::step(stmt)) == SQLITE_ROW ) {
		tapeList.push_back(reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0)));
	}

	sqlite3_statement::checkRcAndFinalize(stmt, rc, SQLITE_DONE);

	return tapeList;
}

void Migration::addRequest()

{
	int rc;
	std::stringstream ssql;
	sqlite3_stmt *stmt;
	int colNumber;
	std::string tapeId;

	ssql << "SELECT COLOC_GRP FROM JOB_QUEUE WHERE REQ_NUM=" << reqNumber << " GROUP BY COLOC_GRP";

	sqlite3_statement::prepare(ssql.str(), &stmt);

	std::stringstream ssql2;
	sqlite3_stmt *stmt2;

	getTapes();

	while ( (rc = sqlite3_statement::step(stmt)) == SQLITE_ROW ) {
		ssql2.str("");
		ssql2.clear();

		colNumber = sqlite3_column_int (stmt, 0);
		tapeId = tapeList[colNumber %tapeList.size()];

		ssql2 << "INSERT INTO REQUEST_QUEUE (OPERATION, REQ_NUM, TARGET_STATE, COLOC_GRP, TAPE_ID, TIME_ADDED, STATE) ";
		ssql2 << "VALUES (" << DataBase::MIGRATION << ", ";                                     // OPERATION
		ssql2 << reqNumber << ", ";                                                             // FILE_NAME
		ssql2 << targetState << ", ";                                                           // TARGET_STATE
		ssql2 << colNumber << ", ";                                                             // COLOC_GRP
		ssql2 << "'" << tapeId << "', ";                                                        // TAPE_ID
		ssql2 << time(NULL) << ", ";                                                            // TIME_ADDED
		ssql2 << DataBase::REQ_NEW << ");";                                                     // STATE

		sqlite3_statement::prepare(ssql2.str(), &stmt2);

		rc = sqlite3_statement::step(stmt2);

		sqlite3_statement::checkRcAndFinalize(stmt2, rc, SQLITE_DONE);


		std::unique_lock<std::mutex> lock(Scheduler::mtx);
		Scheduler::cond.notify_one();
	}

	sqlite3_statement::checkRcAndFinalize(stmt, rc, SQLITE_DONE);
}

bool Migration::queryResult(long reqNumber, long *resident, long *premigrated, long *migrated)

{
	int rc;
	std::stringstream ssql;
	bool done = true;

	ssql << "SELECT STATE FROM REQUEST_QUEUE WHERE REQ_NUM=" << reqNumber;

	sqlite3_statement::prepare(ssql.str(), &stmt);

	while ( (rc = sqlite3_statement::step(stmt)) == SQLITE_ROW ) {
		if ( sqlite3_column_int(stmt, 0) != DataBase::REQ_COMPLETED ) {
			done = false;
		}
	}

	sqlite3_statement::checkRcAndFinalize(stmt, rc, SQLITE_DONE);

	ssql.str("");
	ssql.clear();

	ssql << "SELECT FILE_STATE, COUNT(*) FROM JOB_QUEUE WHERE REQ_NUM=" << reqNumber << " GROUP BY FILE_STATE";

	sqlite3_stmt *stmt;

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
