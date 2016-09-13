#include <time.h>
#include <string.h>
#include <string>
#include <iostream>
#include <iomanip>

#include "trace.h"

int trclevel = 0;

void set_trclevel(int level)

{
	trclevel = level;
}

int get_trclevel()

{
	return trclevel;
}

void trace(const char *filename, int linenr, int dbglvl, const char *varname, const char *varval)

{

	time_t current = time(NULL);
	char curctime[25];

	if ( dbglvl <= trclevel) {
		ctime_r(&current, curctime);
		curctime[strlen(curctime) - 1] = 0;

		std::cout << curctime << ": ";
		std::cout << std::setw(15) << filename;
		std::cout << "(" << linenr << "): ";
		std::cout << varname << "(" << varval << ")" << std::endl;
	}
}
