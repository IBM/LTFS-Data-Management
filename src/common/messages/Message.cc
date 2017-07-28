#include <sys/types.h>
#include <signal.h>
#include <sys/resource.h>

#include <iostream>
#include <fstream>
#include <mutex>

#include "src/common/const/Const.h"
#include "src/common/errors/errors.h"

#include "Message.h"

Message messageObject;

Message::~Message()

{
	try {
		if (messagefile.is_open())
			messagefile.close();
	} catch (...) {
		kill(getpid(), SIGTERM);
	}
}

void Message::init()

{
	messagefile.exceptions(std::ofstream::failbit | std::ofstream::badbit);

	try {
		messagefile.open(Const::LOG_FILE,
				std::fstream::out | std::fstream::app);
	} catch (const std::exception& e) {
		std::cerr << messages[LTFSDMX0003E];
		exit((int) Error::LTFSDM_GENERAL_ERROR);
	}
}

void Message::writeOut(std::string msgstr)

{
	mtx.lock();
	std::cout << msgstr << std::flush;
	mtx.unlock();
}

void Message::writeLog(std::string msgstr)

{
	try {
		mtx.lock();
		messagefile << msgstr;
		messagefile.flush();
		mtx.unlock();
	} catch (const std::exception& e) {
		mtx.unlock();
		std::cerr << messages[LTFSDMX0004E];
		exit((int) Error::LTFSDM_GENERAL_ERROR);
	}
}
