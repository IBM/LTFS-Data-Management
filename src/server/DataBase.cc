#include "ServerIncludes.h"

DataBase DB;

std::mutex DataBase::trans_mutex;

DataBase::~DataBase()

{
    if (dbNeedsClosed)
        sqlite3_close(db);

    sqlite3_shutdown();
}

void DataBase::cleanup()

{
    unlink(Const::DB_FILE.c_str());
    unlink((Const::DB_FILE + "-journal").c_str());
}

void DataBase::fits(sqlite3_context *ctx, int argc, sqlite3_value **argv)

{
    assert(argc == 5);

    unsigned long size = sqlite3_value_int64(argv[1]);
    unsigned long *free = (unsigned long *) sqlite3_value_int64(argv[2]);
    unsigned long *num_found = (unsigned long *) sqlite3_value_int64(argv[3]);
    unsigned long *total = (unsigned long *) sqlite3_value_int64(argv[4]);

    if (*free >= size) {
        *free -= size;
        (*total)++;
        (*num_found)++;
        sqlite3_result_int(ctx, 1);
    } else {
        (*total)++;
        sqlite3_result_int(ctx, 0);
    }
}

void DataBase::open(bool dbUseMemory)

{
    int rc;
    std::string sql;
    std::string uri;

    if (dbUseMemory)
        uri = "file::memory:";
    else
        uri = std::string("file:") + Const::DB_FILE;

    rc = sqlite3_config(SQLITE_CONFIG_URI, 1);

    if (rc != SQLITE_OK) {
        TRACE(Trace::error, rc);
        errno = rc;
        THROW(Error::GENERAL_ERROR, uri, rc);
    }

    rc = sqlite3_initialize();

    if (rc != SQLITE_OK) {
        TRACE(Trace::error, rc);
        errno = rc;
        THROW(Error::GENERAL_ERROR, rc);
    }

    rc = sqlite3_open_v2(uri.c_str(), &db, SQLITE_OPEN_READWRITE |
    SQLITE_OPEN_CREATE | SQLITE_OPEN_FULLMUTEX |
    SQLITE_OPEN_SHAREDCACHE | SQLITE_OPEN_EXCLUSIVE, NULL);

    if (rc != SQLITE_OK) {
        TRACE(Trace::error, rc, uri);
        errno = rc;
        THROW(Error::GENERAL_ERROR, uri, rc);
    }

    rc = sqlite3_extended_result_codes(db, 1);

    if (rc != SQLITE_OK) {
        TRACE(Trace::error, rc);
        errno = rc;
        THROW(Error::GENERAL_ERROR, rc);
    }

    dbNeedsClosed = true;

    sqlite3_create_function(db, "FITS", 5, SQLITE_UTF8, NULL, &DataBase::fits,
    NULL, NULL);
}

void DataBase::createTables()

{
    SQLStatement stmt;

    stmt(DataBase::CREATE_JOB_QUEUE);
    stmt.doall();

    stmt(DataBase::CREATE_REQUEST_QUEUE);
    stmt.doall();
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

SQLStatement& SQLStatement::operator()(std::string fmtstr)

{
    fmt = boost::format(fmtstr);
    return *this;
}

void SQLStatement::prepare()

{
    int rc;

    rc = sqlite3_prepare_v2(DB.getDB(), fmt.str().c_str(), -1, &stmt, NULL);

    if (rc != SQLITE_OK) {
        TRACE(Trace::error, fmt.str(), rc);
        errno = rc;
        THROW(Error::GENERAL_ERROR, rc);
    }
}

void SQLStatement::getColumn(int *result, int column)

{
    *result = sqlite3_column_int(stmt, column);
}

void SQLStatement::getColumn(unsigned int *result, int column)

{
    *result = static_cast<unsigned int>(sqlite3_column_int(stmt, column));
}

void SQLStatement::getColumn(DataBase::operation *result, int column)

{
    *result =
            static_cast<DataBase::operation>(sqlite3_column_int(stmt, column));
}

void SQLStatement::getColumn(DataBase::req_state *result, int column)

{
    *result =
            static_cast<DataBase::req_state>(sqlite3_column_int(stmt, column));
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

void SQLStatement::getColumn(unsigned long long *result, int column)

{
    *result =
            static_cast<unsigned long long>(sqlite3_column_int64(stmt, column));
}

void SQLStatement::getColumn(std::string *result, int column)

{
    const char *column_ctr = reinterpret_cast<const char*>(sqlite3_column_text(
            stmt, column));
    if (column_ctr != NULL)
        *result = std::string(column_ctr);
    else
        *result = "";
}

std::string SQLStatement::str()

{
    return fmt.str();
}

void SQLStatement::bind(int num, int value)

{
    int rc;

    if ((rc = sqlite3_bind_int(stmt, num, value)) != SQLITE_OK) {
        TRACE(Trace::error, rc);
        errno = rc;
        THROW(Error::GENERAL_ERROR, rc);
    }
}

void SQLStatement::bind(int num, std::string value)

{
    int rc;

    if ((rc = sqlite3_bind_text(stmt, num, value.c_str(), value.size(), 0))
            != SQLITE_OK) {
        TRACE(Trace::error, rc);
        errno = rc;
        THROW(Error::GENERAL_ERROR, rc);
    }
}

void SQLStatement::finalize()

{
    if (stmt_rc != SQLITE_ROW && stmt_rc != SQLITE_DONE) {
        TRACE(Trace::error, fmt.str(), stmt_rc);
        errno = stmt_rc;
        THROW(Error::GENERAL_ERROR, stmt_rc);
    }

    int rc = sqlite3_finalize(stmt);

    if (rc != SQLITE_OK) {
        TRACE(Trace::error, fmt.str(), rc);
        errno = rc;
        THROW(Error::GENERAL_ERROR, rc);
    }
}

void SQLStatement::doall()

{
    prepare();
    step();
    finalize();
}
