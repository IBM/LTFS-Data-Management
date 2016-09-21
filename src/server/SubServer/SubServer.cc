#include <string>
#include <vector>
#include <thread>

#include "src/server/ServerComponent/ServerComponent.h"
#include "SubServer.h"

SubServer::~SubServer()

{
	std::vector<std::thread*>::iterator it;

	for(it=components.begin() ; it < components.end(); ++it)
		delete(*it);
}

void SubServer::wait()

{
	std::vector<std::thread*>::iterator it;

	for(it=components.begin() ; it < components.end(); ++it)
		(*it)->join();
}
