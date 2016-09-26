#ifndef _RESPONDER_H
#define _RESPONDER_H

struct ResponderData {
	ResponderData(std::string _label, long _key) : label(_label), key(_key) {}
	std::string label;
	long key;
};

class Responder : public ServerComponent<ResponderData>

{
public:
	Responder(ResponderData _data) : ServerComponent(_data) {}
	~Responder() {};
	void run(ResponderData _data);
};

#endif /* _RESPONDER_H */
