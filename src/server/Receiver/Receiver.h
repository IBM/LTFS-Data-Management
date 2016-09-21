#ifndef _RECEIVER_H
#define _RECEIVER_H

class Receiver : public ServerComponent

{
public:
	Receiver();
	~Receiver() {};
	void run(std::string info);
	std::thread start(std::string info) {return std::thread(&Receiver::run, info);}
};

#endif /* _RECEIVER_H */
