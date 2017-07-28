#include <sys/types.h>
#include <signal.h>
#include <sys/resource.h>

#include <iostream>
#include <fstream>

#include "src/common/const/Const.h"
#include "src/common/messages/Message.h"
#include "src/common/errors/errors.h"

#include "Trace.h"

extern const char *__progname;

Trace traceObject;

Trace::~Trace()

{
    try {
        if (tracefile.is_open())
            tracefile.close();
    } catch (...) {
        kill(getpid(), SIGTERM);
    }
}

void Trace::setTrclevel(traceLevel level)

{
    traceLevel oldLevel;

    switch (level) {
        case Trace::none:
            trclevel = level;
            break;
        case Trace::error:
        case Trace::normal:
        case Trace::full:
            oldLevel = trclevel;
            TRACE(Trace::error, oldLevel, level);
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
        tracefile.open(Const::TRACE_FILE,
                std::fstream::out | std::fstream::app);
    } catch (const std::exception& e) {
        MSG(LTFSDMX0001E);
        exit((int) Error::LTFSDM_GENERAL_ERROR);
    }
}

void Trace::rotate()

{
    tracefile.flush();
    tracefile.close();

    if (std::string(__progname).compare("ltfsdmd") == 0) {
        unlink((Const::TRACE_FILE + ".2").c_str());
        rename((Const::TRACE_FILE + ".1").c_str(),
                (Const::TRACE_FILE + ".2").c_str());
        rename(Const::TRACE_FILE.c_str(), (Const::TRACE_FILE + ".1").c_str());
    }

    tracefile.exceptions(std::ios::failbit | std::ios::badbit);

    try {
        tracefile.open(Const::TRACE_FILE,
                std::fstream::out | std::fstream::app);
    } catch (const std::exception& e) {
        MSG(LTFSDMX0001E);
        exit((int) Error::LTFSDM_GENERAL_ERROR);
    }
}
