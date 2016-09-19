#include <iostream>
#include <fstream>

#include "src/common/const/Const.h"
#include "src/common/errors/errors.h"

#include "Message.h"

Message messageObject;

void Message::redirectToFile()

{
	messagefile.exceptions(std::ios::failbit | std::ios::badbit);

	try {
		messagefile.open(Const::LOG_FILE, std::fstream::out | std::fstream::app);
	}
	catch(...) {
		MSG_OUT(OLTFSX0003E);
		exit((int) OLTFSErr::OLTFS_GENERAL_ERROR);
	}

	toFile = true;
}

Message::~Message()

{
	if (toFile)
		messagefile.close();
}
