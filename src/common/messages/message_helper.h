#ifndef _MESSAGE_HELPER_H
#define _MESSAGE_HELPER_H

#include <stdio.h>
#include <string.h>
#include <string>

template<typename ... Args>
void msg_out(msg_id msg, int linenr, Args ... args )
{
	char msgstr[32768];
	std::string format = msgname[msg].c_str() + std::string("(%d): ") + messages[msg].c_str();
	memset(msgstr, 0, sizeof(msgstr));
	snprintf(msgstr, sizeof(msgstr) -1, format.c_str(), linenr, args ...);
	printf("%s\n", msgstr);
}

#define MSG_OUT(msg, args...) msg_out(msg, __LINE__, ##args)

#endif /* _MESSAGE_HELPER_H */
