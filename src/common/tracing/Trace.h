#ifndef _TRACE_H
#define _TRACE_H

#include <time.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>
#include <string>
#include <iostream>
#include <fstream>
#include <iomanip>

#include "src/common/messages/messages.h"
#include "src/common/errors/errors.h"

class Trace {
private:
	std::ofstream tracefile;
	int trclevel = 0;
public:
	Trace();
	~Trace();

	void setTrclevel(int level);
	int getTrclevel();

	template<typename T>
	void trace(const char *filename, int linenr, int dbglvl, const char *varname, T s)

	{
		time_t current = time(NULL);
		char curctime[25];

		if ( dbglvl <= getTrclevel()) {
			ctime_r(&current, curctime);
			curctime[strlen(curctime) - 1] = 0;

			try {
				tracefile << curctime << ":";
				tracefile << std::setfill('0') << std::setw(6) << getpid() << ":";
#ifdef __linux__
				tracefile << std::setfill('0') << std::setw(6) << gettid() << ":";
#elif __APPLE__
				uint64_t tid;
				pthread_threadid_np(pthread_self(), &tid);
				tracefile << std::setfill('0') << std::setw(6) << tid << ":";
#else
#error "unsupported platform"
#endif
				tracefile << std::setfill('-') << std::setw(15) << filename;
				tracefile << "(" << linenr << "):";
				tracefile << varname << "(" << s << ")" << std::endl;
			}
			catch(...) {
				MSG_OUT(OLTFSX0002E);
				exit((int) OLTFSErr::OLTFS_GENERAL_ERROR);
			}
		}
	}
};

extern Trace T;

void set_trclevel(int level);
int get_trclevel();

#define TRACE(dbglvl, var) T.trace(__FILE__, __LINE__, dbglvl, #var, var)

#endif /*_TRACE_H */
