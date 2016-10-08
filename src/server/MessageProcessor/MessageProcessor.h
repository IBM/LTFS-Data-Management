#ifndef _MESSAGEPROCESSOR_H
#define _MESSAGEPROCESSOR_H

class MessageProcessor

{
private:
	void getObjects(LTFSDmCommServer *command, long localReqNumber, unsigned long pid, long requestNumber);
	void migrationMessage(long key, LTFSDmCommServer *command, long localReqNumber);
	void selRecallMessage(long key, LTFSDmCommServer *command, long localReqNumber);
	void requestNumber(long key, LTFSDmCommServer *command, long *localReqNumber);
	void stop(long key, LTFSDmCommServer *command, long localReqNumber);
	void status(long key, LTFSDmCommServer *command, long localReqNumber);
public:
	std::mutex termmtx;
	std::condition_variable termcond;
	MessageProcessor() {}
	~MessageProcessor() {};
	void run(std::string label, long key, LTFSDmCommServer command);
};

#endif /* _MESSAGEPROCESSOR_H */
