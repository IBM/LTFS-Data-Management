#ifndef _MESSAGEPARSER_H
#define _MESSAGEPARSER_H

class MessageParser

{
private:
	void getObjects(LTFSDmCommServer *command, long localReqNumber,
					unsigned long pid, long requestNumber, FileOperation *fopt);
	void reqStatusMessage(long key, LTFSDmCommServer *command, FileOperation *fopt);
	void reqStatusMessage(long key, LTFSDmCommServer *command);
	void migrationMessage(long key, LTFSDmCommServer *command, long localReqNumber);
	void selRecallMessage(long key, LTFSDmCommServer *command, long localReqNumber);
	void infoFilesMessage(long key, LTFSDmCommServer *command, long localReqNumber);
	void requestNumber(long key, LTFSDmCommServer *command, long *localReqNumber);
	void stopMessage(long key, LTFSDmCommServer *command, long localReqNumber);
	void statusMessage(long key, LTFSDmCommServer *command, long localReqNumber);
	void addMessage(long key, LTFSDmCommServer *command, long localReqNumber);
public:
	std::mutex termmtx;
	std::condition_variable termcond;
	MessageParser() {}
	~MessageParser() {};
	void run(long key, LTFSDmCommServer command);
};

#endif /* _MESSAGEPARSER_H */
