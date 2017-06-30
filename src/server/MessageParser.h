#ifndef _MESSAGEPARSER_H
#define _MESSAGEPARSER_H

class MessageParser

{
private:
	static void getObjects(LTFSDmCommServer *command, long localReqNumber,
					unsigned long pid, long requestNumber, FileOperation *fopt);
	static void reqStatusMessage(long key, LTFSDmCommServer *command, FileOperation *fopt);
	static void reqStatusMessage(long key, LTFSDmCommServer *command);
	static void migrationMessage(long key, LTFSDmCommServer *command, long localReqNumber);
	static void selRecallMessage(long key, LTFSDmCommServer *command, long localReqNumber);
	static void infoFilesMessage(long key, LTFSDmCommServer *command, long localReqNumber);
	static void requestNumber(long key, LTFSDmCommServer *command, long *localReqNumber);
	static void stopMessage(long key, LTFSDmCommServer *command, long localReqNumber);
	static void statusMessage(long key, LTFSDmCommServer *command, long localReqNumber);
	static void addMessage(long key, LTFSDmCommServer *command, long localReqNumber, Connector *connector);
	static void infoRequestsMessage(long key, LTFSDmCommServer *command, long localReqNumber);
	static void infoJobsMessage(long key, LTFSDmCommServer *command, long localReqNumber);
	static void infoDrivesMessage(long key, LTFSDmCommServer *command);
	static void infoTapesMessage(long key, LTFSDmCommServer *command);
	static void poolCreateMessage(long key, LTFSDmCommServer *command);
	static void poolDeleteMessage(long key, LTFSDmCommServer *command);
	static void poolAddMessage(long key, LTFSDmCommServer *command);
	static void poolRemoveMessage(long key, LTFSDmCommServer *command);
	static void infoPoolsMessage(long key, LTFSDmCommServer *command);
	static void retrieveMessage(long key, LTFSDmCommServer *command);
public:
	MessageParser() {}
	~MessageParser() {};
	static void run(long key, LTFSDmCommServer command, Connector *connector);
};

#endif /* _MESSAGEPARSER_H */
