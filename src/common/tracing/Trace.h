#ifndef _TRACE_H
#define _TRACE_H

#include <time.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>
#include <string>
#include <iostream>
#include <iomanip>

void set_trclevel(int level);
int get_trclevel();

template<typename T>
void trace(const char *filename, int linenr, int dbglvl, const char *varname, T s)

{
	time_t current = time(NULL);
	char curctime[25];

	if ( dbglvl <= get_trclevel()) {
		ctime_r(&current, curctime);
		curctime[strlen(curctime) - 1] = 0;

		std::cerr << curctime << ":";
		std::cerr << std::setfill('0') << std::setw(6) << getpid() << ":";
#ifdef __linux__
		std::cerr << std::setfill('0') << std::setw(6) << gettid() << ":";
#elif __APPLE__
		uint64_t tid;
		pthread_threadid_np(pthread_self(), &tid);
		std::cerr << std::setfill('0') << std::setw(6) << tid << ":";
#else
#error "unsupported platform"
#endif
		std::cerr << std::setfill('-') << std::setw(15) << filename;
		std::cerr << "(" << linenr << "):";
		std::cerr << varname << "(" << s << ")" << std::endl;
	}
}

#define TRACE(dbglvl, var) trace(__FILE__, __LINE__, dbglvl, #var, var)

#endif /*_TRACE_H */
