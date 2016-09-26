#ifndef _SERVERCOMPONENT_H
#define _SERVERCOMPONENT_H

#include <thread>

template <typename TD>
class ServerComponent

{
protected:
	TD data;
public:
	ServerComponent(TD _data) : data(_data) {}
	~ServerComponent() {};

	template <typename TC>
	std::thread *startThread(TC *s)
	{
		return new std::thread(&TC::run, s, s->getInfo());
	}

	TD getInfo() { return data; }
	virtual void run(TD _data) {};
};

#endif /* _SERVERCOMPONENT_H */
