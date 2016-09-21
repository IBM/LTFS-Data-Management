#ifndef _RESPONDER_H
#define _RESPONDER_H

class Responder : public ServerComponent<std::string>

{
public:
	Responder(std::string _info)  { info = _info; }
	~Responder() {};
	void run(std::string info);
};

#endif /* _RESPONDER_H */
