#ifndef _SELRECALL_H
#define _SELRECALL_H

class SelRecall : public FileOperation {
private:
	unsigned long pid;
	long reqNumber;
	LTFSDmProtocol::LTFSDmSelRecRequest::State targetState;
public:
	SelRecall(unsigned long _pid, long _reqNumber, LTFSDmProtocol::LTFSDmSelRecRequest::State _targetState) :
		pid(_pid), reqNumber(_reqNumber), targetState(_targetState) {}
	void addFileName(std::string fileName);
	void start();
};

#endif /* _SELRECALL_H */
