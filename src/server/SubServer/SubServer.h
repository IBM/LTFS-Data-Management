#ifndef _SUBSERVER_H
#define _SUBSERVER_H

class SubServer {
private:
	std::vector<std::thread*> components;
public:
	SubServer() {};
	~SubServer();

	template <typename T>
	void add(T s)
	{
		components.push_back(s->startThread(s));
	}
	void wait();
};

#endif /*_SUBSERVER_H */
