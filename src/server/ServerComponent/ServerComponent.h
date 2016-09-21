#ifndef _SERVERCOMPONENT_H
#define _SERVERCOMPONENT_H

#include <thread>

class ServerComponent

{
protected:
	std::string info;
public:
	ServerComponent();
	~ServerComponent() {};

	template <typename T>
	std::thread *startThread(T *s)
	{
		return new std::thread(&T::run, s, s->getInfo());
	}

	std::string getInfo() { return info; }
	void run(std::string info) {};
};

#endif /* _SERVERCOMPONENT_H */
