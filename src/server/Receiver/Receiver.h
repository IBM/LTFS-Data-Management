#ifndef _RECEIVER_H
#define _RECEIVER_H

struct ReceiverData {
	ReceiverData(std::string _label, long _key) : label(_label), key(_key) {}
	std::string label;
	long key;
};

class Receiver : public ServerComponent<ReceiverData>

{
public:
	Receiver(ReceiverData _data) : ServerComponent(_data) {}
	~Receiver() {};
	void run(ReceiverData _data);
};

#endif /* _RECEIVER_H */
