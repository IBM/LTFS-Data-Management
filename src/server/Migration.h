#ifndef _MIGRATION_H
#define _MIGRATION_H

class Migration : public FileOperation {
private:
	unsigned long pid;
	long reqNumber;
	int colFactor;
	LTFSDmProtocol::LTFSDmMigRequest::State targetState;
	int jobnum;
	std::vector<std::string> tapeList;
	std::vector<std::string> getTapes();
public:
	Migration(unsigned long _pid, long _reqNumber, int _colFactor,
			  LTFSDmProtocol::LTFSDmMigRequest::State _targetState) :
		pid(_pid), reqNumber(_reqNumber), colFactor(_colFactor),
		targetState(_targetState), jobnum(0) {}
	void addJob(std::string fileName);
	void addRequest();
	static void execRequest(int reqNum, int tgtState, int colGrp, std::string tapeId);
};

#endif /* _MIGRATION_H */
