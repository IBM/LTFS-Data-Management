#ifndef _DATABASE_H
#define _DATABASE_H

class DataBase {
private:
	sqlite3 *db;
	bool dbNeedsClosed;
public:
	enum operation {
		MIGRATION,
		RECALL
	};
	DataBase() : db(NULL), dbNeedsClosed(false) {}
	~DataBase();
	void cleanup();
	void open();
	void createTables();
	sqlite3 *getDB() { return db; }
};

extern DataBase DB;

#endif /* _DATABASE_H */
