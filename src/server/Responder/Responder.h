#ifndef _RESPONDER_H
#define _RESPONDER_H

class Responder : public ServerComponent<long>

{
public:
	Responder(long _info)  { info = _info; }
	~Responder() {};
	void run(long key);
};

#endif /* _RESPONDER_H */
