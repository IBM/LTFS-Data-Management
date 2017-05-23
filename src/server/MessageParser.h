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
	void addMessage(long key, LTFSDmCommServer *command, long localReqNumber, Connector *connector);
	void infoRequestsMessage(long key, LTFSDmCommServer *command, long localReqNumber);
	void infoJobsMessage(long key, LTFSDmCommServer *command, long localReqNumber);
	void infoDrivesMessage(long key, LTFSDmCommServer *command);
	void infoTapesMessage(long key, LTFSDmCommServer *command);
	void poolCreateMessage(long key, LTFSDmCommServer *command);
	void poolDeleteMessage(long key, LTFSDmCommServer *command);
	void poolAddMessage(long key, LTFSDmCommServer *command);
	void poolRemoveMessage(long key, LTFSDmCommServer *command);
	void infoPoolsMessage(long key, LTFSDmCommServer *command);
	void retrieveMessage(long key, LTFSDmCommServer *command);
public:
	MessageParser() {}
	~MessageParser() {};
	void run(long key, LTFSDmCommServer command, Connector *connector);
};

#endif /* _MESSAGEPARSER_H */
