#ifndef _SERVERCOMPONENT_H
#define _SERVERCOMPONENT_H

#include <thread>

template <typename TD>
class ServerComponent

{
protected:
	TD info;
public:
	ServerComponent() {};
	~ServerComponent() {};

	template <typename TC>
	std::thread *startThread(TC *s)
	{
		return new std::thread(&TC::run, s, s->getInfo());
	}

	TD getInfo() { return info; }
	virtual void run(TD _info) {};
};

#endif /* _SERVERCOMPONENT_H */
