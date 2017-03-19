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
	std::vector<std::string> tapeList;
	std::vector<std::string> getTapes();
	std::vector<Migration> migs;

	unsigned long preMigrate(std::string fileName, std::string tapeId, long secs, long nsecs);
	void stub(std::string fileName);
	bool migrationStep(int replNum, int colGrp, std::string tapeId, int fromState, int toState);
public:
	Migration(unsigned long _pid, long _reqNumber, int _numReplica, int _colFactor,
			  LTFSDmProtocol::LTFSDmMigRequest::State _targetState) :
		pid(_pid), reqNumber(_reqNumber), numReplica(_numReplica),
		colFactor(_colFactor), targetState(_targetState), jobnum(0) {}
	void addJob(std::string fileName);
	void addRequest();
	void execRequest(int tgtState, int replNum, int colGrp, std::string tapeId);
};

#endif /* _MIGRATION_H */
