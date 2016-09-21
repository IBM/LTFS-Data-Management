#ifndef _RESPONDER_H
#define _RESPONDER_H

class Responder : public ServerComponent

{
public:
	Responder();
	~Responder() {};
	void run(std::string info);
	std::thread start(std::string info) {return std::thread(&Responder::run, info);}
};

#endif /* _RESPONDER_H */
