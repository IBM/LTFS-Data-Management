#pragma once

#include <stdio.h>
#include <time.h>
#include <unistd.h>
#include <string.h>
#include <libgen.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/syscall.h>
#include <fstream>
#include <iomanip>
#include <atomic>
#include <mutex>

#include "src/common/messages/Message.h"
#include "src/common/exception/OpenLTFSException.h"
#include "src/common/errors/errors.h"

class Trace
{
private:
    std::mutex mtx;
    int fd;
    std::string fileName;
public:
    enum traceLevel
    {
        none, always, error, normal, full
    };
private:
    std::atomic<Trace::traceLevel> trclevel;

    void processParms(std::stringstream *stream)
    {
        *stream << "";
    }

    template<typename T>
    void processParms(std::stringstream *stream, std::string varlist, T s)
    {
        *stream << varlist.substr(varlist.find_first_not_of(" "),
                varlist.size()) << "(" << s << ")";
    }

    template<typename T, typename ... Args>
    void processParms(std::stringstream *stream, std::string varlist, T s, Args ... args)
    {
        *stream << varlist.substr(varlist.find_first_not_of(" "),
                varlist.find(',')) << "(" << s << "), ";
        processParms(stream, varlist.substr(varlist.find(',') + 1, varlist.size()),
                args ...);
    }
public:
    Trace() :
            fd(Const::UNSET), fileName(Const::TRACE_FILE), trclevel(error)
    {
    }
    ~Trace();

    void init(std::string extension = "");
    void rotate();

    void setTrclevel(traceLevel level);
    int getTrclevel();

    template<typename ... Args>
    void trace(const char *filename, int linenr, traceLevel tl,
            std::string varlist, Args ... args)

    {
        struct timeval curtime;
        struct tm tmval;
        std::stringstream stream;
        char curctime[26];

        if (getTrclevel() > none && tl <= getTrclevel()) {
            try {
                gettimeofday(&curtime, NULL);
                localtime_r(&(curtime.tv_sec), &tmval);
                strftime(curctime, sizeof(curctime) - 1, "%Y-%m-%dT%H:%M:%S",
                        &tmval);
                stream << curctime << "." << std::setfill('0')
                        << std::setw(6) << curtime.tv_usec << ":["
                        << std::setfill('0') << std::setw(6) << getpid() << ":"
                        << std::setfill('0') << std::setw(6)
                        << syscall(SYS_gettid) << "]:" << std::setfill('-')
                        << std::setw(20) << basename((char *) filename) << "("
                        << std::setfill('0') << std::setw(4) << linenr << "): ";
                processParms(&stream, varlist, args ...);
                stream << std::endl;

                rotate();
                if ( write(fd,stream.str().c_str(),stream.str().size() )
                            != stream.str().size() )
                        throw(EXCEPTION(errno));
            } catch (const std::exception& e) {
                MSG(LTFSDMX0002E);
                exit((int) Error::LTFSDM_GENERAL_ERROR);
            }
        }
    }
};

extern Trace traceObject;

#define TRACE(tracelevel, args ...) traceObject.trace(__FILE__, __LINE__, tracelevel, #args, args)
