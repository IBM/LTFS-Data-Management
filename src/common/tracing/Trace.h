#ifndef _TRACE_H
#define _TRACE_H

#include <time.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
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
		time_t current = time(NULL);
		char curctime[25];

		if ( tl <= getTrclevel()) {
			ctime_r(&current, curctime);
			curctime[strlen(curctime) - 1] = 0;

			try {
				mtx.lock();
				tracefile << curctime << ":";
				tracefile << std::setfill('0') << std::setw(6) << getpid() << ":";
#ifdef __linux__
				tracefile << std::setfill('0') << std::setw(6) << syscall(SYS_gettid) << ":";
#elif __APPLE__
				uint64_t tid;
				pthread_threadid_np(pthread_self(), &tid);
				tracefile << std::setfill('0') << std::setw(6) << tid << ":";
#else
#error "unsupported platform"
#endif
				tracefile << std::setfill('-') << std::setw(20) << filename;
				tracefile << "(" << linenr << "):";
				tracefile << varname << "(" << s << ")" << std::endl;
				tracefile.flush();
				mtx.unlock();
			}
			catch(...) {
				mtx.unlock();
				MSG(LTFSDMX0002E);
				exit((int) LTFSDMErr::LTFSDM_GENERAL_ERROR);
			}
		}
	}
};

extern Trace traceObject;

#define TRACE(tracelevel, var) traceObject.trace(__FILE__, __LINE__, tracelevel, #var, var)

#endif /*_TRACE_H */
