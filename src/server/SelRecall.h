#ifndef _SELRECALL_H
#define _SELRECALL_H

class SelRecall : public FileOperation {
private:
	unsigned long pid;
	long reqNumber;
	std::set<std::string> needsTape;
	LTFSDmProtocol::LTFSDmSelRecRequest::State targetState;
	static unsigned long recall(std::string fileName, std::string tapeId,
								FsObj::file_state state, FsObj::file_state toState);
	static bool recallStep(int reqNumber, std::string tapeId, FsObj::file_state toState, bool needsTape);
public:
	SelRecall(unsigned long _pid, long _reqNumber,
			  LTFSDmProtocol::LTFSDmSelRecRequest::State _targetState) :
		pid(_pid), reqNumber(_reqNumber), targetState(_targetState) {}
	void addJob(std::string fileName);
	void addRequest();
	static void execRequest(int reqNum, int tgtState, std::string tapeId, bool needsTape);
};

#endif /* _SELRECALL_H */
