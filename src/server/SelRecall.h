#ifndef _SELRECALL_H
#define _SELRECALL_H

class SelRecall : public FileOperation {
private:
	unsigned long pid;
	long reqNumber;
	LTFSDmProtocol::LTFSDmSelRecRequest::State targetState;
	long getStartBlock(std::string tapeName);
public:
	SelRecall(unsigned long _pid, long _reqNumber,
			  LTFSDmProtocol::LTFSDmSelRecRequest::State _targetState) :
		pid(_pid), reqNumber(_reqNumber), targetState(_targetState) {}
	void addJob(std::string fileName);
	void addRequest();
	static void execRequest(int reqNum, int tgtState, std::string tapeId);
};

#endif /* _SELRECALL_H */
