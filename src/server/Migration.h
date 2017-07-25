#pragma once

class Migration : public FileOperation {
private:
	unsigned long pid;
	long reqNumber;
	std::set<std::string> pools;
	int numReplica;
	LTFSDmProtocol::LTFSDmMigRequest::State targetState;
	int jobnum;
	bool needsTape = false;
    struct req_return_t {
		bool remaining;
		bool suspended;
	};

	FsObj::file_state checkState(std::string fileName, FsObj *fso);

    static const std::string ADD_MIGRATION_JOB;
	static const std::string ADD_MIGRATION_REQUEST;
	static const std::string FAIL_PREMIGRATION;
	static const std::string FAIL_STUBBING;
	static const std::string SET_PREMIGRATING;
	static const std::string SET_STUBBING;
	static const std::string SELECT_MIG_JOBS;
	static const std::string SET_MIG_SUCCESS;
	static const std::string SET_MIG_RESET;
	static const std::string FAIL_PREMIGRATED;
	static const std::string UPDATE_MIG_REQUEST;
	static const std::string UPDATE_MIG_REQUEST_RESET_TAPE;

public:
	struct mig_info_t {
		std::string fileName;
		int reqNumber;
		int numRepl;
		int replNum;
		unsigned long inum;
		std::string poolName;
		FsObj::file_state fromState;
		FsObj::file_state toState;
	};
	static std::mutex pmigmtx;

private:
	static std::string genInumString(std::shared_ptr<std::list<unsigned long>> inumList);
	static req_return_t migrationStep(int reqNumber, int numRepl, int replNum, std::string tapeId,
									  FsObj::file_state fromState, FsObj::file_state toState);
public:
	Migration(unsigned long _pid, long _reqNumber, std::set<std::string> _pools,
		  int _numReplica, LTFSDmProtocol::LTFSDmMigRequest::State _targetState) :
	pid(_pid), reqNumber(_reqNumber), pools(_pools), numReplica(_numReplica),
	    targetState(_targetState), jobnum(0) {}
	void addJob(std::string fileName);
	void addRequest();
	static unsigned long preMigrate(std::string tapeId, std::string driveId, long secs, long nsecs,
									mig_info_t miginfo, std::shared_ptr<std::list<unsigned long>> inumList,
									std::shared_ptr<bool>);
	static void stub(mig_info_t mig_info, std::shared_ptr<std::list<unsigned long>> inumList);
	static void execRequest(int reqNumber, int targetState, int numRepl, int replNum,
							std::string pool, std::string tapeId, bool needsTape);
};
