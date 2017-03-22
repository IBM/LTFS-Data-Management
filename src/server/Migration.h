#ifndef _MIGRATION_H
#define _MIGRATION_H

class Migration : public FileOperation {
private:
	unsigned long pid;
	long reqNumber;
	int numReplica;
	int colFactor;
	LTFSDmProtocol::LTFSDmMigRequest::State targetState;
	int jobnum;
	bool needsTape = false;
	std::vector<std::string> tapeList;
	std::vector<std::string> getTapes();

	static unsigned long preMigrate(std::string fileName, std::string tapeId, long secs, long nsecs, int numRepl);
	static void stub(std::string fileName, int numRepl);
	static bool migrationStep(int reqNumber, int numRepl, int replNum, int colGrp, std::string tapeId, FsObj::file_state fromState, FsObj::file_state toState);
public:
	Migration(unsigned long _pid, long _reqNumber, int _numReplica, int _colFactor,
			  LTFSDmProtocol::LTFSDmMigRequest::State _targetState) :
		pid(_pid), reqNumber(_reqNumber), numReplica(_numReplica),
		colFactor(_colFactor), targetState(_targetState), jobnum(0) {}
	void addJob(std::string fileName);
	void addRequest();
	static void execRequest(int reqNumber, int targetState, int numRepl,
							int replNum, int colGrp, std::string tapeId, bool needsTape);
};

#endif /* _MIGRATION_H */
