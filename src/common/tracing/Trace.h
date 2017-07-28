#pragma once

#include <time.h>
#include <unistd.h>
#include <string.h>
#include <libgen.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/syscall.h>
#include <string>
#include <iostream>
#include <fstream>
#include <iomanip>
#include <atomic>
#include <mutex>

#include "src/common/messages/Message.h"
#include "src/common/errors/errors.h"

class Trace
{
private:
    std::mutex mtx;
    std::ofstream tracefile;
public:
    enum traceLevel
    {
        none, always, error, normal, full
    };
private:
    std::atomic<Trace::traceLevel> trclevel;

    std::string recurse()
    {
        return "";
    }

    template<typename T>
    void recurse(std::string varlist, T s)
    {
        tracefile
                << varlist.substr(varlist.find_first_not_of(" "),
                        varlist.size()) << "(" << s << ")";
    }

    template<typename T, typename ... Args>
    void recurse(std::string varlist, T s, Args ... args)
    {
        tracefile
                << varlist.substr(varlist.find_first_not_of(" "),
                        varlist.find(',')) << "(" << s << "), ";
        recurse(varlist.substr(varlist.find(',') + 1, varlist.size()),
                args ...);
    }
public:
    Trace() :
            trclevel(error)
    {
    }
    ~Trace();

    void init();
    void rotate();

    void setTrclevel(traceLevel level);
    int getTrclevel();

    template<typename ... Args>
    void trace(const char *filename, int linenr, traceLevel tl,
            std::string varlist, Args ... args)

    {
        struct timeval curtime;
        struct tm tmval;
        ;
        char curctime[26];

        if (getTrclevel() > none && tl <= getTrclevel()) {
            gettimeofday(&curtime, NULL);
            localtime_r(&(curtime.tv_sec), &tmval);
            strftime(curctime, sizeof(curctime) - 1, "%Y-%m-%dT%H:%M:%S",
                    &tmval);

            try {
                mtx.lock();

                if (tracefile.tellp() > 100 * 1024 * 1024)
                    rotate();

                tracefile << curctime << "." << std::setfill('0')
                        << std::setw(6) << curtime.tv_usec << ":["
                        << std::setfill('0') << std::setw(6) << getpid() << ":"
                        << std::setfill('0') << std::setw(6)
                        << syscall(SYS_gettid) << "]:" << std::setfill('-')
                        << std::setw(20) << basename((char *) filename) << "("
                        << std::setfill('0') << std::setw(4) << linenr << "): ";
                recurse(varlist, args ...);
                tracefile << std::endl;
                tracefile.flush();
                mtx.unlock();
            } catch (const std::exception& e) {
                mtx.unlock();
                MSG(LTFSDMX0002E);
                exit((int) Error::LTFSDM_GENERAL_ERROR);
            }
        }
    }
};

extern Trace traceObject;

#define TRACE(tracelevel, args ...) traceObject.trace(__FILE__, __LINE__, tracelevel, #args, args)
