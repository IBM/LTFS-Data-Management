#ifndef _DATABASE_H
#define _DATABASE_H

class DataBase {
private:
	sqlite3 *db;
	bool dbNeedsClosed;
public:
	enum operation {
		TRARECALL,
		SELRECALL,
		MIGRATION
	};
	enum req_state {
		REQ_NEW,
		REQ_INPROGRESS,
		REQ_COMPLETED
	};
	enum tape_state {
		TAPE_FREE,
		TAPE_INUSE
	};
	enum file_state {
		RESIDENT,
		PREMIGRATED,
		MIGRATED
	};
	DataBase() : db(NULL), dbNeedsClosed(false) {}
	~DataBase();
	void cleanup();
	void open();
	void createTables();
	sqlite3 *getDB() { return db; }
};

extern DataBase DB;

namespace sqlite3_statement {
	void prepare(std::string sql, sqlite3_stmt **stmt);
	int step(sqlite3_stmt *stmt);
	void checkRcAndFinalize(sqlite3_stmt *stmt, int rc, int expected);
}

#endif /* _DATABASE_H */
