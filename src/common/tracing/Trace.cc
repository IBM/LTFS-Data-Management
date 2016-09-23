#include <iostream>
#include <fstream>

#include "src/common/const/Const.h"
#include "src/common/messages/Message.h"
#include "src/common/errors/errors.h"

#include "Trace.h"

Trace traceObject;

void Trace::setTrclevel(traceLevel level)

{
	traceLevel oldLevel = trclevel;
	TRACE(Trace::error, oldLevel);
	TRACE(Trace::error, level);
	trclevel = level;
}

int Trace::getTrclevel()

{
	return trclevel;
}

Trace::Trace() : trclevel(error)

{
	tracefile.exceptions(std::ios::failbit | std::ios::badbit);

	try {
		tracefile.open(Const::TRACE_FILE, std::fstream::out | std::fstream::app);
	}
	catch(...) {
		MSG(LTFSDMX0001E);
		exit((int) LTFSDMErr::LTFSDM_GENERAL_ERROR);
	}
}

Trace::~Trace()

{
	tracefile.close();
}
