#include <iostream>
#include <fstream>

#include "src/common/const/Const.h"
#include "src/common/errors/errors.h"

#include "Message.h"

Message messageObject;

Message::Message()

{
	messagefile.exceptions(std::ios::failbit | std::ios::badbit);

	try {
		messagefile.open(Const::LOG_FILE, std::fstream::out | std::fstream::app);
	}
	catch(...) {
		std::cerr << OLTFSX0003E;
		exit((int) OLTFSErr::OLTFS_GENERAL_ERROR);
	}
}

Message::~Message()

{
	messagefile.close();
}

void Message::writeOut(std::string msgstr)

{
	std::cout << msgstr;
}

void Message::writeLog(std::string msgstr)

{
	try {
		messagefile << msgstr;
		messagefile.flush();
	}
	catch(...) {
		std::cerr << OLTFSX0004E;
		exit((int) OLTFSErr::OLTFS_GENERAL_ERROR);
	}
}
