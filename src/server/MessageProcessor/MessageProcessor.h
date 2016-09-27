#ifndef _MESSAGEPROCESSOR_H
#define _MESSAGEPROCESSOR_H

struct MessageProcessorData {
	MessageProcessorData(std::string _label, long _key, LTFSDmCommServer *_command) : label(_label), key(_key), command(_command) {}
	std::string label;
	long key;
	LTFSDmCommServer *command;
};

class MessageProcessor : public ServerComponent<MessageProcessorData>

{
private:
	void migrationMessage(long key, LTFSDmCommServer *command);
	void selRecallMessage(long key, LTFSDmCommServer *command);
	void requestNumber(long key, LTFSDmCommServer *command);

public:
	MessageProcessor(MessageProcessorData _data) : ServerComponent(_data) {}
	~MessageProcessor() {};
	void run(MessageProcessorData _data);
};

#endif /* _MESSAGEPROCESSOR_H */
