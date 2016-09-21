#include <string>
#include <vector>
#include <thread>

#include "src/server/ServerComponent/ServerComponent.h"
#include "SubServer.h"

SubServer::SubServer()

{
}

void SubServer::start(ServerComponent *component, std::string info)

{
	std::thread thread = component->start(info);
	components.push_back(thrd);
}

void SubServer::wait()

{
	std::vector<std::thread>::iterator it;

	for(it=components.begin() ; it < components.end(); ++it)
		(*it).join();
}
