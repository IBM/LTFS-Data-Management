#include <pthread.h>

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
	pthread_mutex_lock(&mtx);
	std::cout << msgstr;
	pthread_mutex_unlock(&mtx);
}

void Message::writeLog(std::string msgstr)

{
	try {
		pthread_mutex_lock(&mtx);
		messagefile << msgstr;
		messagefile.flush();
		pthread_mutex_unlock(&mtx);
	}
	catch(...) {
		std::cerr << OLTFSX0004E;
		exit((int) OLTFSErr::OLTFS_GENERAL_ERROR);
	}
}
