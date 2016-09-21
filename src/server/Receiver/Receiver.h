#ifndef _RECEIVER_H
#define _RECEIVER_H

class Receiver : public ServerComponent

{
public:
	Receiver(std::string _info) { info = _info; }
	~Receiver() {};
	void run(std::string _info);
};

#endif /* _RECEIVER_H */
