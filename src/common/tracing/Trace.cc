#include <iostream>
#include <fstream>

#include "src/common/const/Const.h"
#include "src/common/messages/messages.h"
#include "src/common/errors/errors.h"

#include "Trace.h"

Trace T;

void set_trclevel(int level)

{
	T.setTrclevel(level);
}

int get_trclevel()

{
	return T.getTrclevel();
}


void Trace::setTrclevel(int level)

{
	trclevel = level;
}

int Trace::getTrclevel()

{
	return trclevel;
}

Trace::Trace()

{
	tracefile.exceptions(std::ios::failbit | std::ios::badbit);

	try {
		tracefile.open(Const::TRACE_FILE, std::fstream::out | std::fstream::app);
	}
	catch(...) {
		MSG_OUT(OLTFSX0001E);
		exit((int) OLTFSErr::OLTFS_GENERAL_ERROR);
	}
}

Trace::~Trace()

{
	tracefile.close();
}
