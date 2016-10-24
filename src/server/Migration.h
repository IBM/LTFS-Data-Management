#ifndef _MIGRATION_H
#define _MIGRATION_H

class Migration : public FileOperation {
private:
	unsigned long pid;
	long reqNumber;
	int colFactor;
	LTFSDmProtocol::LTFSDmMigRequest::State targetState;
	sqlite3_stmt *stmt;
	int jobnum;
	std::vector<std::string> tapeList;
	std::vector<std::string> getTapes();
public:
	Migration(unsigned long _pid, long _reqNumber, int _colFactor, LTFSDmProtocol::LTFSDmMigRequest::State _targetState) :
		pid(_pid), reqNumber(_reqNumber), colFactor(_colFactor), targetState(_targetState), stmt(NULL), jobnum(0) {}
	void addFileName(std::string fileName);
	void addRequest();
	bool queryResult(long reqNumber, long *resident, long *premigrated, long *migrated);
};

#endif /* _MIGRATION_H */
