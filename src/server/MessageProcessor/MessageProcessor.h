#ifndef _MESSAGEPROCESSOR_H
#define _MESSAGEPROCESSOR_H

class MessageProcessor

{
private:
	void migrationMessage(long key, LTFSDmCommServer *command);
	void selRecallMessage(long key, LTFSDmCommServer *command);
	void requestNumber(long key, LTFSDmCommServer *command);
	void stop(long key, LTFSDmCommServer *command);

public:
	MessageProcessor() {}
	~MessageProcessor() {};
	void run(std::string label, long key, LTFSDmCommServer *command);
};

#endif /* _MESSAGEPROCESSOR_H */
