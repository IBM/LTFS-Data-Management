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
