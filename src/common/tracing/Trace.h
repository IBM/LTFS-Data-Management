#ifndef _TRACE_H
#define _TRACE_H

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

class Trace {
private:
	std::mutex mtx;
	std::ofstream tracefile;
public:
	enum traceLevel {
		error,
		little,
		medium,
		much
	};
private:
	std::atomic<Trace::traceLevel> trclevel;
public:
	Trace() : trclevel(error) {}
	~Trace();

	void init();

	void setTrclevel(traceLevel level);
	int getTrclevel();

	template<typename T>
	void trace(const char *filename, int linenr, traceLevel tl, const char *varname, T s)

	{
		struct timeval curtime;
		struct tm tmval;;
		char curctime[26];

		if ( tl <= getTrclevel()) {
			gettimeofday(&curtime, NULL);
			localtime_r(&(curtime.tv_sec), &tmval);
			strftime(curctime, sizeof(curctime) - 1, "%Y-%m-%dT%H:%M:%S", &tmval);

			try {
				mtx.lock();
				tracefile << curctime << ".";
				tracefile << std::setfill('0') << std::setw(6) << curtime.tv_usec << ":[";
				tracefile << std::setfill('0') << std::setw(6) << getpid() << ":";
#ifdef __linux__
				tracefile << std::setfill('0') << std::setw(6) << syscall(SYS_gettid) << "]:";
#elif __APPLE__
				uint64_t tid;
				pthread_threadid_np(pthread_self(), &tid);
				tracefile << std::setfill('0') << std::setw(6) << tid << "]:";
#else
#error "unsupported platform"
#endif
				tracefile << std::setfill('-') << std::setw(20) << basename((char *) filename);
				tracefile << "(" << std::setfill('0') << std::setw(4) << linenr << "):";
				tracefile << varname << "(" << s << ")" << std::endl;
				tracefile.flush();
				mtx.unlock();
			}
			catch(...) {
				mtx.unlock();
				MSG(LTFSDMX0002E);
				exit((int) Error::LTFSDM_GENERAL_ERROR);
			}
		}
	}
};

extern Trace traceObject;

#define TRACE(tracelevel, var) traceObject.trace(__FILE__, __LINE__, tracelevel, #var, var)

#endif /*_TRACE_H */
