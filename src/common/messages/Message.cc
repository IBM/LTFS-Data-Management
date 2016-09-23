#include <iostream>
#include <fstream>
#include <mutex>

#include "src/common/const/Const.h"
#include "src/common/errors/errors.h"

#include "Message.h"

Message messageObject;

Message::Message() : logType(Message::STDOUT)

{
	messagefile.exceptions(std::ios::failbit | std::ios::badbit);

	try {
		messagefile.open(Const::LOG_FILE, std::fstream::out | std::fstream::app);
	}
	catch(...) {
		std::cerr << LTFSDMX0003E;
		exit((int) LTFSDMErr::LTFSDM_GENERAL_ERROR);
	}
}

Message::~Message()

{
	messagefile.close();
}

void Message::writeOut(std::string msgstr)

{
	mtx.lock();
	std::cout << msgstr;
	mtx.unlock();
}

void Message::writeLog(std::string msgstr)

{
	try {
		mtx.lock();
		messagefile << msgstr;
		messagefile.flush();
		mtx.unlock();
	}
	catch(...) {
		mtx.unlock();
		std::cerr << LTFSDMX0004E;
		exit((int) LTFSDMErr::LTFSDM_GENERAL_ERROR);
	}
}
