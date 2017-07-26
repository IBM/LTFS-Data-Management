#pragma once

class DataBase {
private:
	sqlite3 *db;
	bool dbNeedsClosed;
	static void fits(sqlite3_context *ctx, int argc, sqlite3_value **argv);
public:
	enum operation {
		TRARECALL,
		SELRECALL,
		MIGRATION,
		NOOP
	};
	enum req_state {
		REQ_NEW,
		REQ_INPROGRESS,
		REQ_COMPLETED
	};
	static std::mutex trans_mutex;
	DataBase() : db(NULL), dbNeedsClosed(false) {}
	~DataBase();
	void cleanup();
	void open(bool dbUseMemory);
	void createTables();
	int lastUpdates();
	sqlite3 *getDB() { return db; }
	static std::string opStr(operation op);
	static std::string reqStateStr(req_state reqs);
};

extern DataBase DB;

namespace sqlite3_statement {
	void prepare(std::string sql, sqlite3_stmt **stmt);
	int step(sqlite3_stmt *stmt);
	void checkRcAndFinalize(sqlite3_stmt *stmt, int rc, int expected);
}


class SQLStatement {
private:
	sqlite3_stmt *stmt;
	std::string stmt_str;
	int stmt_rc;

	void getColumn(int *result, int column);
	void getColumn(unsigned int *result, int column);
	void getColumn(DataBase::operation *result, int column);
	void getColumn(FsObj::file_state *result, int column);
	void getColumn(long *result, int column);
	void getColumn(unsigned long *result, int column);
	void getColumn(unsigned long long *result, int column);
	void getColumn(std::string *result, int column);

	void eval(int column) {}

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
	SQLStatement() : stmt(nullptr), stmt_str(""), stmt_rc(0) {}
	SQLStatement(boost::basic_format<char>& fmt) : stmt_str(fmt.str()) {}
	~SQLStatement() {}

	void operator<< (boost::basic_format<char>& fmt) { stmt_str = fmt.str(); }
	std::string str();
	void bind(int num, int value);
	void bind(int num, std::string value);
	void prepare();
	template<typename ... Args>
	bool step(Args ... args)
	{
		int column = 0;

		stmt_rc = sqlite3_step(stmt);

		if ( stmt_rc != SQLITE_ROW )
			return false;

		eval(column, args ...);

		return true;
	}
	void finalize();
	void doall();
};
