#ifndef _MIGRATION_H
#define _MIGRATION_H

class Migration : public FileOperation {
private:
	unsigned long pid;
	long reqNumber;
	std::set<std::string> pools;
	int numReplica;
	LTFSDmProtocol::LTFSDmMigRequest::State targetState;
	int jobnum;
	bool needsTape = false;

public:
	struct mig_info_t {
		std::string fileName;
		int reqNumber;
		int numRepl;
		int replNum;
		std::string poolName;
		FsObj::file_state fromState;
		FsObj::file_state toState;
	};

private:
	static unsigned long preMigrate(std::string tapeId, long secs, long nsecs, mig_info_t miginfo);
	static void stub(mig_info_t mig_info);
	static bool migrationStep(int reqNumber, int numRepl, int replNum, std::string tapeId, FsObj::file_state fromState, FsObj::file_state toState);
public:
Migration(unsigned long _pid, long _reqNumber, std::set<std::string> _pools,
		  int _numReplica, LTFSDmProtocol::LTFSDmMigRequest::State _targetState) :
	pid(_pid), reqNumber(_reqNumber), pools(_pools), numReplica(_numReplica),
	    targetState(_targetState), jobnum(0) {}
	void addJob(std::string fileName);
	void addRequest();
	static void execRequest(int reqNumber, int targetState, int numRepl, int replNum,
							std::string pool, std::string tapeId, bool needsTape);
};

#endif /* _MIGRATION_H */
