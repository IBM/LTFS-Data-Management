#ifndef _TRACING_H
#define _TRACING_H

#include <string.h>
#include <time.h>
#include <string>
#include <iostream>

int TRACELVL = 0;

template<typename ... Args>
void trace(const char *filename, int tracelvl, int linenr, const char *format,  Args ... args )
{
	if (tracelvl > TRACELVL )
		return;

	char msgstr[32768];
	char timebuf[64];
	time_t now = time(NULL);

	memset(now, 0, sizeof(now));
	memset(msgstr, 0, sizeof(msgstr));

	std::string fullformat = std::string("%s %10s:%d ") + std::string(format);
	snprintf(msgstr, sizeof(msgstr) -1, fullformat.c_str(), ctime_r(now, timebuf), filename, linenr, args ...);
	std::cout << msgstr;
}

#define TRACE(tracelvl, format, args...) trace(__FILE__, __LINE__, format, ##args)

#endif /* _TRACING_H */
