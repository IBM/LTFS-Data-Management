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
