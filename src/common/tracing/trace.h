#ifndef _TRACE_H
#define _TRACE_H

#include <time.h>
#include <string.h>
#include <string>
#include <iostream>
#include <iomanip>

void set_trclevel(int level);
int get_trclevel();

template<typename ... Args>
void trace(const char *filename, int linenr, int dbglvl, const char *format, Args ... args )
{
	char msgstr[32768];
	time_t current = time(NULL);
	char curctime[25];

	if ( dbglvl <= get_trclevel()) {
		ctime_r(&current, curctime);
		curctime[strlen(curctime) - 1] = 0;

		memset(msgstr, 0, sizeof(msgstr));
		snprintf(msgstr, sizeof(msgstr) -1, format, args ...);
		std::cout << curctime << ": ";
		std::cout << std::setw(15) << filename;
		std::cout << "(" << linenr << "): " << msgstr;
	}
}

#define TRACE(dbglvl, format, args...) trace(__FILE__, __LINE__, dbglvl, format, ##args)

#endif /*_TRACE_H */
