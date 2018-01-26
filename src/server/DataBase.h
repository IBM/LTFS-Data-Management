#pragma once

class DataBase
{
private:
    sqlite3 *db;
    bool dbNeedsClosed;
    static void fits(sqlite3_context *ctx, int argc, sqlite3_value **argv);
    static const std::string CREATE_JOB_QUEUE;
    static const std::string CREATE_REQUEST_QUEUE;
public:
    enum operation
    {
        TRARECALL, /**@< 0 */
        SELRECALL, /**@< 1 */
        MIGRATION, /**@< 2 */
        NOOP /**@< 3 */
    };
    enum req_state
    {
        REQ_NEW, /**@< 0 */
        REQ_INPROGRESS, /**@< 1 */
        REQ_COMPLETED /**@< 2 */
    };
    static std::mutex trans_mutex;
    DataBase() :
            db(NULL), dbNeedsClosed(false)
    {
    }
    ~DataBase();
    void cleanup();
    void open(bool dbUseMemory);
    void createTables();
    int lastUpdates();
    sqlite3 *getDB()
    {
        return db;
    }
    static std::string opStr(operation op);
    static std::string reqStateStr(req_state reqs);
};

extern DataBase DB;

class SQLStatement
{
private:
    sqlite3_stmt *stmt;
    boost::format fmt;
    int stmt_rc;

    std::string encode(std::string s);
    std::string decode(std::string s);
    void getColumn(int *result, int column);
    void getColumn(unsigned int *result, int column);
    void getColumn(DataBase::operation *result, int column);
    void getColumn(DataBase::req_state *result, int column);
    void getColumn(FsObj::file_state *result, int column);
    void getColumn(long *result, int column);
    void getColumn(unsigned long *result, int column);
    void getColumn(unsigned long long *result, int column);
    void getColumn(std::string *result, int column);

    void eval(int column)
    {
    }

    template<typename T>
    void eval(int column, T s)
    {
        getColumn(s, column);
    }

    template<typename T, typename ... Args>
    void eval(int column, T s, Args ... args)
    {
        getColumn(s, column);
        column++;
        eval(column, args ...);
    }

public:
    SQLStatement() :
            stmt(nullptr), fmt(""), stmt_rc(0)
    {
    }
    SQLStatement(std::string fmtstr) :
            stmt(nullptr), fmt(boost::format(fmtstr)), stmt_rc(0)
    {
    }
    SQLStatement& operator()(std::string fmtstr);
    ~SQLStatement()
    {
    }

    // convert unsigned to signed since there is unsigned in SQLite
    SQLStatement& operator<<(unsigned long long llu)
    {
        fmt % static_cast<long>(llu);
        return *this;
    }

    SQLStatement& operator<<(unsigned long lu)
    {
        fmt % static_cast<long>(lu);
        return *this;
    }

    SQLStatement& operator<<(std::string s)
    {
        fmt % encode(s);
        return *this;
    }

    SQLStatement& operator<<(unsigned int u)
    {
        fmt % static_cast<int>(u);
        return *this;
    }

    template<typename T>
    SQLStatement& operator<<(T s)
    {
        fmt % (s);
        return *this;
    }

    std::string str();
    void bind(int num, int value);
    void bind(int num, std::string value);
    void prepare();

    template<typename ... Args>
    bool step(Args ... args)
    {
        int column = 0;

        stmt_rc = sqlite3_step(stmt);

        if (stmt_rc != SQLITE_ROW)
            return false;

        eval(column, args ...);

        return true;
    }

    void finalize();
    void doall();
};
