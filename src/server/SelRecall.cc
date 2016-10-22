#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include <iostream>
#include <sstream>

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

#include "SelRecall.h"

void SelRecall::addFileName(std::string fileName)

{
	int rc;
	struct stat statbuf;
	std::stringstream ssql;
	sqlite3_stmt *stmt;

	ssql << "INSERT INTO JOB_QUEUE (OPERATION, FILE_NAME, REQ_NUM, TARGET_STATE, COLOC_GRP, FILE_SIZE, FS_ID, I_GEN, I_NUM, MTIME, LAST_UPD, TAPE_ID, FAILED) ";
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
		ssql << 0 << ");";                                        // FAILED
	}
	catch ( int error ) {
		MSG(LTFSDMS0017E, fileName.c_str());
		return;
	}


	rc = sqlite3_prepare_v2(DB.getDB(), ssql.str().c_str(), -1, &stmt, NULL);

	if( rc != SQLITE_OK ) {
		TRACE(Trace::error, rc);
		throw(rc);
	}

	rc = sqlite3_step(stmt);

	if ( rc != SQLITE_DONE ) {
		TRACE(Trace::error, rc);
		throw(rc);
	}

	rc = sqlite3_finalize(stmt);

	if ( rc != SQLITE_OK ) {
		TRACE(Trace::error, rc);
		throw(rc);
	}

	return;
}

void SelRecall::start()

{
	int rc;
	std::stringstream ssql;
	sqlite3_stmt *stmt;

	ssql.str("");
	ssql.clear();

	ssql << "SELECT TAPE_ID FROM JOB_QUEUE WHERE REQ_NUM=" << reqNumber << " GROUP BY TAPE_ID";

	rc = sqlite3_prepare_v2(DB.getDB(), ssql.str().c_str(), -1, &stmt, NULL);

	if( rc != SQLITE_OK ) {
		TRACE(Trace::error, rc);
		throw(rc);
	}

	std::stringstream ssql2;
	sqlite3_stmt *stmt2;

	while ( (rc = sqlite3_step(stmt)) == SQLITE_ROW ) {
		ssql2.str("");
		ssql2.clear();

		const char *cstr = reinterpret_cast<const char*>(sqlite3_column_text (stmt, 0));

		ssql2 << "INSERT INTO REQUEST_QUEUE (OPERATION, REQ_NUM, TARGET_STATE, COLOC_GRP, TAPE_ID, TIME_ADDED, STATE) ";
		ssql2 << "VALUES (" << DataBase::SELRECALL << ", ";                                     // OPERATION
		ssql2 << reqNumber << ", ";                                                             // FILE_NAME
		ssql2 << targetState << ", ";                                                           // TARGET_STATE
		ssql2 << "NULL" << ", ";                                                                // COLOC_GRP
		ssql2 << "'" << (cstr ? std::string(cstr) : std::string("")) << "', ";                  // TAPE_ID
		ssql2 << time(NULL) << ", ";                                                            // TIME_ADDED
		ssql2 << DataBase::NEW << ");";                                                         // STATE

		rc = sqlite3_prepare_v2(DB.getDB(), ssql2.str().c_str(), -1, &stmt2, NULL);

		if( rc != SQLITE_OK ) {
			TRACE(Trace::error, rc);
			throw(rc);
		}

		rc = sqlite3_step(stmt2);

		if ( rc != SQLITE_DONE ) {
			TRACE(Trace::error, rc);
			throw(rc);
		}

		rc = sqlite3_finalize(stmt2);

		if ( rc != SQLITE_OK ) {
			TRACE(Trace::error, rc);
			throw(rc);
		}
	}
}
