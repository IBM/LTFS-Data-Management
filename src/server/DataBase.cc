#include <unistd.h>
#include <sys/resource.h>

#include <string>
#include <sstream>
#include <set>

#include <sqlite3.h>

#include "src/common/util/util.h"
#include "src/common/messages/Message.h"
#include "src/common/tracing/Trace.h"
#include "src/common/errors/errors.h"
#include "src/common/const/Const.h"

#include "DataBase.h"

DataBase DB;

DataBase::~DataBase()

{
	if ( dbNeedsClosed)
		sqlite3_close(db);

	sqlite3_shutdown();
}

void DataBase::cleanup()

{
	unlink(Const::DB_FILE.c_str());
	unlink((Const::DB_FILE + std::string("-journal")).c_str());
}

void DataBase::open()

{
	int rc;
	std::string sql;
	std::string uri = std::string("file:") + Const::DB_FILE;

	rc = sqlite3_config(SQLITE_CONFIG_URI,1);

	if ( rc != SQLITE_OK ) {
		TRACE(Trace::error, rc);
		throw(Error::LTFSDM_GENERAL_ERROR);
	}

	rc = sqlite3_initialize();

	if ( rc != SQLITE_OK ) {
		TRACE(Trace::error, rc);
		throw(Error::LTFSDM_GENERAL_ERROR);
	}

	rc = sqlite3_open_v2(uri.c_str(), &db, SQLITE_OPEN_READWRITE |
						 SQLITE_OPEN_CREATE | SQLITE_OPEN_FULLMUTEX |
						 SQLITE_OPEN_SHAREDCACHE | SQLITE_OPEN_EXCLUSIVE, NULL);

	if ( rc != SQLITE_OK ) {
		TRACE(Trace::error, rc);
		TRACE(Trace::error, uri);
		throw(Error::LTFSDM_GENERAL_ERROR);
	}

	rc = sqlite3_extended_result_codes(db, 1);

	if ( rc != SQLITE_OK ) {
		TRACE(Trace::error, rc);
		throw(Error::LTFSDM_GENERAL_ERROR);
	}

	dbNeedsClosed = true;
}

void DataBase::createTables()

{
	std::stringstream ssql;
	sqlite3_stmt *stmt;
	int rc;

	ssql << "CREATE TABLE JOB_QUEUE("
		 << "OPERATION INT NOT NULL, "
		 << "FILE_NAME CHAR(4096), "
		 << "REQ_NUM INT NOT NULL, "
		 << "TARGET_STATE INT NOT NULL, "
		 << "REPL_NUM INT, "
		 << "COLOC_GRP INT, "
		 << "FILE_SIZE INT NOT NULL, "
		 << "FS_ID BIGINT NOT NULL, "
		 << "I_GEN INT NOT NULL, "
		 << "I_NUM BIGINT NOT NULL, "
		 << "MTIME_SEC BIGINT NOT NULL, "
		 << "MTIME_NSEC BIGINT NOT NULL, "
		 << "LAST_UPD INT NOT NULL, "
		 << "TAPE_ID CHAR(9), "
		 << "FILE_STATE INT NOT NULL, "
		 << "START_BLOCK INT, "
		 << "CONN_INFO BIGINT UNIQUE, "
		 << "CONSTRAINT JOB_QUEUE_UNIQUE_FILE_NAME UNIQUE (FILE_NAME, REPL_NUM), "
		 << "CONSTRAINT JOB_QUEUE_UNIQUE_UID UNIQUE (FS_ID, I_GEN, I_NUM, REPL_NUM));";

	sqlite3_statement::prepare(ssql.str(), &stmt);

	rc = sqlite3_statement::step(stmt);

	sqlite3_statement::checkRcAndFinalize(stmt, rc, SQLITE_DONE);

	ssql.str("");
	ssql.clear();

	ssql << "CREATE TABLE REQUEST_QUEUE("
		 << "OPERATION INT NOT NULL, "
		 << "REQ_NUM INT NOT NULL, "
		 << "TARGET_STATE INT, "
		 << "NUM_REPL, "
		 << "REPL_NUM INT, "
		 << "COLOC_GRP INT, "
		 << "TAPE_ID CHAR(9), "
		 << "TIME_ADDED INT NOT NULL, "
		 << "STATE INT NOT NULL, "
		 << "CONSTRAINT REQUEST_QUEUE_UNIQUE UNIQUE(REQ_NUM, REPL_NUM, COLOC_GRP, TAPE_ID));";

	sqlite3_statement::prepare(ssql.str(), &stmt);

	rc = sqlite3_statement::step(stmt);

	sqlite3_statement::checkRcAndFinalize(stmt, rc, SQLITE_DONE);

	ssql.str("");
	ssql.clear();

	ssql << "CREATE TABLE TAPE_LIST("
		 << "TAPE_ID CHAR(9), "
		 << "STATE INT NOT NULL);";

	sqlite3_statement::prepare(ssql.str(), &stmt);

	rc = sqlite3_statement::step(stmt);

	sqlite3_statement::checkRcAndFinalize(stmt, rc, SQLITE_DONE);

	/* temporary for this prototype add two specific tapes initalially */

	ssql.str("");
	ssql.clear();

	ssql << "INSERT INTO TAPE_LIST (TAPE_ID, STATE) VALUES "
		 << "('DV1480L6', " << DataBase::TAPE_FREE << "), "
		 << "('DV1481L6', " << DataBase::TAPE_FREE << ");";

	sqlite3_statement::prepare(ssql.str(), &stmt);

	rc = sqlite3_statement::step(stmt);

	sqlite3_statement::checkRcAndFinalize(stmt, rc, SQLITE_DONE);
}

void DataBase::beginTransaction()
{
	int rc;

	rc = sqlite3_exec(db, "BEGIN TRANSACTION;", NULL, NULL, NULL);

	if( rc != SQLITE_OK ) {
		TRACE(Trace::error, rc);
		throw(rc);
	}
}

void DataBase::endTransaction()
{
	int rc;

	rc = sqlite3_exec(db, "END TRANSACTION;", NULL, NULL, NULL);

	if( rc != SQLITE_OK ) {
		TRACE(Trace::error, rc);
		throw(rc);
	}
}

void sqlite3_statement::prepare(std::string sql, sqlite3_stmt **stmt)

{
	int rc;

	rc = sqlite3_prepare_v2(DB.getDB(), sql.c_str(), -1, stmt, NULL);

	if( rc != SQLITE_OK ) {
		TRACE(Trace::error, sql);
		TRACE(Trace::error, rc);
		throw(rc);
	}
}

int sqlite3_statement::step(sqlite3_stmt *stmt)

{
	return  sqlite3_step(stmt);
}

void sqlite3_statement::checkRcAndFinalize(sqlite3_stmt *stmt, int rc, int expected)

{
	std::string statement(sqlite3_sql(stmt));

	if ( rc != expected ) {
		TRACE(Trace::error, statement);
		TRACE(Trace::error, rc);
		throw(rc);
	}

	rc = sqlite3_finalize(stmt);

	if ( rc != SQLITE_OK ) {
		TRACE(Trace::error, statement);
		TRACE(Trace::error, rc);
		throw(rc);
	}
}
