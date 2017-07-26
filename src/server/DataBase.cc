#include "ServerIncludes.h"

DataBase DB;

std::mutex DataBase::trans_mutex;

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

void DataBase::fits(sqlite3_context *ctx, int argc, sqlite3_value **argv)

{
	assert(argc == 5);

	unsigned long size = sqlite3_value_int64(argv[1]);
	unsigned long *free = (unsigned long *) sqlite3_value_int64(argv[2]);
	unsigned long *num_found = (unsigned long *) sqlite3_value_int64(argv[3]);
	unsigned long *total = (unsigned long *) sqlite3_value_int64(argv[4]);

	if ( *free  >= size ) {
		*free -= size;
		(*total)++;
		(*num_found)++;
		sqlite3_result_int(ctx, 1);
	}
	else {
		(*total)++;
		sqlite3_result_int(ctx, 0);
	}
}

void DataBase::open(bool dbUseMemory)

{
	int rc;
	std::string sql;
	std::string uri;

	if ( dbUseMemory )
		uri = "file::memory:";
	else
		uri = std::string("file:") + Const::DB_FILE;

	rc = sqlite3_config(SQLITE_CONFIG_URI,1);

	if ( rc != SQLITE_OK ) {
		TRACE(Trace::error, rc);
		throw(EXCEPTION(Const::UNSET, uri, rc));
	}

	rc = sqlite3_initialize();

	if ( rc != SQLITE_OK ) {
		TRACE(Trace::error, rc);
		throw(EXCEPTION(Const::UNSET, rc));
	}

	rc = sqlite3_open_v2(uri.c_str(), &db, SQLITE_OPEN_READWRITE |
						 SQLITE_OPEN_CREATE | SQLITE_OPEN_FULLMUTEX |
						 SQLITE_OPEN_SHAREDCACHE | SQLITE_OPEN_EXCLUSIVE, NULL);

	if ( rc != SQLITE_OK ) {
		TRACE(Trace::error, rc, uri);
		throw(EXCEPTION(Const::UNSET, uri, rc));
	}

	rc = sqlite3_extended_result_codes(db, 1);

	if ( rc != SQLITE_OK ) {
		TRACE(Trace::error, rc);
		throw(EXCEPTION(Const::UNSET, rc));
	}

	dbNeedsClosed = true;

	sqlite3_create_function(db, "FITS", 5, SQLITE_UTF8, NULL, &DataBase::fits, NULL, NULL);
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
		 << "TAPE_POOL VARCHAR, "
		 << "FILE_SIZE BIGINT NOT NULL, "
		 << "FS_ID BIGINT NOT NULL, "
		 << "I_GEN INT NOT NULL, "
		 << "I_NUM BIGINT NOT NULL, "
		 << "MTIME_SEC BIGINT NOT NULL, "
		 << "MTIME_NSEC BIGINT NOT NULL, "
		 << "LAST_UPD INT NOT NULL, "
		 << "TAPE_ID CHAR(9), "
		 << "FILE_STATE INT NOT NULL, "
		 << "START_BLOCK INT, "
		 << "CONN_INFO BIGINT, "
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
		 << "TAPE_POOL VARCHAR, "
		 << "TAPE_ID CHAR(9), "
		 << "TIME_ADDED INT NOT NULL, "
		 << "STATE INT NOT NULL, "
		 << "CONSTRAINT REQUEST_QUEUE_UNIQUE UNIQUE(REQ_NUM, REPL_NUM, TAPE_POOL, TAPE_ID));";

	sqlite3_statement::prepare(ssql.str(), &stmt);

	rc = sqlite3_statement::step(stmt);

	sqlite3_statement::checkRcAndFinalize(stmt, rc, SQLITE_DONE);
}

std::string DataBase::opStr(DataBase::operation op)

{
	switch (op) {
		case TRARECALL:
			return messages[LTFSDMX0015I];
		case SELRECALL:
			return messages[LTFSDMX0014I];
		case MIGRATION:
			return messages[LTFSDMX0013I];
		default:
			return "";
	}
}


std::string DataBase::reqStateStr(DataBase::req_state reqs)

{
	switch (reqs) {
		case REQ_NEW:
			return messages[LTFSDMX0016I];
		case REQ_INPROGRESS:
			return messages[LTFSDMX0017I];
		case REQ_COMPLETED:
			return messages[LTFSDMX0018I];
		default:
			return "";
	}
}


int DataBase::lastUpdates()

{
	return sqlite3_changes(db);
}

// OLD: -- begin --

void sqlite3_statement::prepare(std::string sql, sqlite3_stmt **stmt)

{
	int rc;

	rc = sqlite3_prepare_v2(DB.getDB(), sql.c_str(), -1, stmt, NULL);

	if( rc != SQLITE_OK ) {
		TRACE(Trace::error, sql, rc);
		throw(EXCEPTION(rc, rc));
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
		TRACE(Trace::error, statement, rc);
		throw(EXCEPTION(rc, rc));
	}

	rc = sqlite3_finalize(stmt);

	if ( rc != SQLITE_OK ) {
		TRACE(Trace::error, statement, rc);
		throw(EXCEPTION(rc, rc));
	}
}


// OLD: -- END --


void SQLStatement::prepare()

{
	int rc;

	rc = sqlite3_prepare_v2(DB.getDB(), stmt_str.c_str(), -1, &stmt, NULL);

	if( rc != SQLITE_OK ) {
		TRACE(Trace::error, stmt_str, rc);
		throw(EXCEPTION(rc, rc));
	}
}

void SQLStatement::getColumn(int *result, int column)

{
	*result = sqlite3_column_int(stmt, column);
}

void SQLStatement::getColumn(DataBase::operation *result, int column)

{
	*result = static_cast<DataBase::operation>(sqlite3_column_int(stmt, column));
}

void SQLStatement::getColumn(FsObj::file_state *result, int column)

{
	*result = static_cast<FsObj::file_state>(sqlite3_column_int(stmt, column));
}

void SQLStatement::getColumn(long *result, int column)

{
	*result = sqlite3_column_int64(stmt, column);
}

void SQLStatement::getColumn(unsigned long *result, int column)

{
	*result = static_cast<unsigned long>(sqlite3_column_int64(stmt, column));
}

void SQLStatement::getColumn(std::string *result, int column)

{
	const char *column_ctr = reinterpret_cast<const char*>(sqlite3_column_text (stmt, column));
	if ( column_ctr != NULL)
		*result = std::string(column_ctr);
	else
		*result = "";
}

std::string SQLStatement::str()

{
	return stmt_str;
}


void SQLStatement::bind(int num, int value)

{
	int rc;

	if ( (rc = sqlite3_bind_int(stmt, num, value)) != SQLITE_OK ) {
		TRACE(Trace::error, rc);
		EXCEPTION(rc);
	}
}

void SQLStatement::bind(int num, std::string value)

{
	int rc;

	if ( (rc = sqlite3_bind_text(stmt, num, value.c_str(), value.size(), 0)) != SQLITE_OK ) {
		TRACE(Trace::error, rc);
		EXCEPTION(rc);
	}
}

void SQLStatement::finalize()

{
	if ( stmt_rc != SQLITE_ROW &&
		 stmt_rc != SQLITE_DONE ) {
		TRACE(Trace::error, stmt_str, stmt_rc);
		throw(EXCEPTION(stmt_rc, stmt_rc));
	}

	int rc = sqlite3_finalize(stmt);

	if ( rc != SQLITE_OK ) {
		TRACE(Trace::error, stmt_str, rc);
		throw(EXCEPTION(rc, rc));
	}
}

void SQLStatement::doall()

{
	prepare();
	step();
	finalize();
}
