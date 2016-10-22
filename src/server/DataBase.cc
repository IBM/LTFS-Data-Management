#include <unistd.h>

#include <string>
#include <sstream>

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
		throw(LTFSDMErr::LTFSDM_GENERAL_ERROR);
	}

	rc = sqlite3_initialize();

	if ( rc != SQLITE_OK ) {
		TRACE(Trace::error, rc);
		throw(LTFSDMErr::LTFSDM_GENERAL_ERROR);
	}

	rc = sqlite3_open_v2(uri.c_str(), &db, SQLITE_OPEN_READWRITE |
						 SQLITE_OPEN_CREATE | SQLITE_OPEN_FULLMUTEX |
						 SQLITE_OPEN_SHAREDCACHE | SQLITE_OPEN_EXCLUSIVE, NULL);

	if ( rc != SQLITE_OK ) {
		TRACE(Trace::error, rc);
		TRACE(Trace::error, uri);
		throw(LTFSDMErr::LTFSDM_GENERAL_ERROR);
	}

	rc = sqlite3_extended_result_codes(db, 1);

	if ( rc != SQLITE_OK ) {
		TRACE(Trace::error, rc);
		throw(LTFSDMErr::LTFSDM_GENERAL_ERROR);
	}

	dbNeedsClosed = true;
}

void DataBase::createTables()

{
	std::string sql;
	sqlite3_stmt *stmt;
	int rc;

	sql = std::string("CREATE TABLE JOB_QUEUE(")
		+ std::string("OPERATION INT NOT NULL, ")
// "NULLs are still distinct in a UNIQUE column" => good for transparent recall
		+ std::string("FILE_NAME CHAR(4096) UNIQUE PRIMARY KEY, ")
		+ std::string("REQ_NUM INT NOT NULL, ")
		+ std::string("TARGET_STATE INT NOT NULL, ")
		+ std::string("COLOC_GRP INT, ")
		+ std::string("FILE_SIZE INT NOT NULL, ")
		+ std::string("FS_ID INT NOT NULL, ")
		+ std::string("I_GEN INT NOT NULL, ")
		+ std::string("I_NUM INT NOT NULL, ")
		+ std::string("MTIME INT NOT NULL, ")
		+ std::string("LAST_UPD INT NOT NULL, ")
		+ std::string("TAPE_ID CHAR(9), ")
		+ std::string("FAILED INT NOT NULL);");

	rc = sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, NULL);

	if( rc != SQLITE_OK ) {
		TRACE(Trace::error, rc);
		throw(LTFSDMErr::LTFSDM_GENERAL_ERROR);
	}

	rc = sqlite3_step(stmt);

	if ( rc != SQLITE_DONE ) {
		TRACE(Trace::error, rc);
		throw(LTFSDMErr::LTFSDM_GENERAL_ERROR);
	}

	rc = sqlite3_finalize(stmt);

	if ( rc != SQLITE_OK ) {
		TRACE(Trace::error, rc);
		throw(LTFSDMErr::LTFSDM_GENERAL_ERROR);
	}

	sql = std::string("CREATE TABLE REQUEST_QUEUE(")
		+ std::string("OPERATION INT NOT NULL, ")
		+ std::string("REQ_NUM INT NOT NULL, ")
		+ std::string("TARGET_STATE INT, ")
		+ std::string("COLOC_GRP INT, ")
		+ std::string("TAPE_ID CHAR(9), ")
		+ std::string("TIME_ADDED INT NOT NULL, ")
		+ std::string("STATE INT NOT NULL);");

	rc = sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, NULL);

	if( rc != SQLITE_OK ) {
		TRACE(Trace::error, rc);
		throw(LTFSDMErr::LTFSDM_GENERAL_ERROR);
	}

	rc = sqlite3_step(stmt);

	if ( rc != SQLITE_DONE ) {
		TRACE(Trace::error, rc);
		throw(LTFSDMErr::LTFSDM_GENERAL_ERROR);
	}

	rc = sqlite3_finalize(stmt);

	if ( rc != SQLITE_OK ) {
		TRACE(Trace::error, rc);
		throw(LTFSDMErr::LTFSDM_GENERAL_ERROR);
	}

	sql = std::string("CREATE TABLE TAPE_LIST(")
		+ std::string("TAPE_ID CHAR(9), ")
		+ std::string("STATE INT NOT NULL);");

	rc = sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, NULL);

	if( rc != SQLITE_OK ) {
		TRACE(Trace::error, rc);
		throw(LTFSDMErr::LTFSDM_GENERAL_ERROR);
	}

	rc = sqlite3_step(stmt);

	if ( rc != SQLITE_DONE ) {
		TRACE(Trace::error, rc);
		throw(LTFSDMErr::LTFSDM_GENERAL_ERROR);
	}

	rc = sqlite3_finalize(stmt);

	if ( rc != SQLITE_OK ) {
		TRACE(Trace::error, rc);
		throw(LTFSDMErr::LTFSDM_GENERAL_ERROR);
	}

	/* temporary for this prototype add two specific tapes initalially */

	std::stringstream ssql;

	ssql << "INSERT INTO TAPE_LIST (TAPE_ID, STATE) VALUES ";
	ssql << "('DV1480L6', " << DataBase::FREE << "), ";
	ssql << "('DV1481L6', " << DataBase::FREE << ");";

	rc = sqlite3_prepare_v2(db, ssql.str().c_str(), -1, &stmt, NULL);

	if( rc != SQLITE_OK ) {
		TRACE(Trace::error, rc);
		throw(LTFSDMErr::LTFSDM_GENERAL_ERROR);
	}

	rc = sqlite3_step(stmt);

	if ( rc != SQLITE_DONE ) {
		TRACE(Trace::error, rc);
		throw(LTFSDMErr::LTFSDM_GENERAL_ERROR);
	}

	rc = sqlite3_finalize(stmt);

	if ( rc != SQLITE_OK ) {
		TRACE(Trace::error, rc);
		throw(LTFSDMErr::LTFSDM_GENERAL_ERROR);
	}

}
