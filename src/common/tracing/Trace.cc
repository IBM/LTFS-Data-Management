#include <sys/resource.h>

#include <iostream>
#include <fstream>

#include "src/common/const/Const.h"
#include "src/common/messages/Message.h"
#include "src/common/errors/errors.h"

#include "Trace.h"

Trace traceObject;

Trace::~Trace()

{
	if ( tracefile.is_open() )
		tracefile.close();
}

void Trace::setTrclevel(traceLevel level)

{
	traceLevel oldLevel;

	switch (level) {
		case Trace::error:
		case Trace::little:
		case Trace::medium:
        case Trace::much:
			oldLevel = trclevel;
			TRACE(Trace::error, oldLevel);
			TRACE(Trace::error, level);
			trclevel = level;
			break;
		default:
			trclevel = Trace::error;
			break;
	}
}

int Trace::getTrclevel()

{
	return trclevel;
}

void Trace::init()

{
	tracefile.exceptions(std::ios::failbit | std::ios::badbit);

	try {
		tracefile.open(Const::TRACE_FILE, std::fstream::out | std::fstream::app);
	}
	catch(...) {
		MSG(LTFSDMX0001E);
		exit((int) Error::LTFSDM_GENERAL_ERROR);
	}
}
