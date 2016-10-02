#ifndef _MESSAGEPROCESSOR_H
#define _MESSAGEPROCESSOR_H

class MessageProcessor

{
private:
	void migrationMessage(long key, LTFSDmCommServer *command, bool *cont);
	void selRecallMessage(long key, LTFSDmCommServer *command, bool *cont);
	void requestNumber(long key, LTFSDmCommServer *command, bool *cont);
	void stop(long key, LTFSDmCommServer *command, bool *cont);
	void status(long key, LTFSDmCommServer *command, bool *cont);
public:
	std::mutex termmtx;
	std::condition_variable termcond;
	MessageProcessor() {}
	~MessageProcessor() {};
	void run(std::string label, long key, LTFSDmCommServer command);
};

#endif /* _MESSAGEPROCESSOR_H */
