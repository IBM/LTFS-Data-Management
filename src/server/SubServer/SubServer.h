#ifndef _SUBSERVER_H
#define _SUBSERVER_H

class SubServer {
private:
	std::vector<std::thread> components;
public:
	SubServer();
	~SubServer() {};

	void start(ServerComponent *component, std::string info);
	void wait();
};

#endif /*_SUBSERVER_H */
