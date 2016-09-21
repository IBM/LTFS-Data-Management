#ifndef _SERVERCOMPONENT_H
#define _SERVERCOMPONENT_H

class ServerComponent

{
public:
	ServerComponent();
	~ServerComponent() {};
	virtual void run(std::string info);
	virtual std::thread start(std::string info);
};

#endif /* _SERVERCOMPONENT_H */
