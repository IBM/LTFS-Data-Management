#ifndef _TRACE_H
#define _TRACE_H

void set_trclevel(int level);
int get_trclevel();
void trace(const char *filename, int linenr, int dbglvl, const char *varname, const char *varval );

#define TRACE(dbglvl, var) trace(__FILE__, __LINE__, dbglvl, #var, (var))

#endif /*_TRACE_H */
