#ifndef _MESSAGE_HELPER_H
#define _MESSAGE_HELPER_H

#include <string.h>
#include <string>
#include <iostream>

template<typename ... Args>
void msg_out(msg_id msg, int linenr, Args ... args )
{
	char msgstr[32768];
	std::string format = msgname[msg] + std::string("(%d): ") + messages[msg];
	memset(msgstr, 0, sizeof(msgstr));
	snprintf(msgstr, sizeof(msgstr) -1, format.c_str(), linenr, args ...);
	std::cout << msgstr;
}

template<typename ... Args>
void msg_info(msg_id msg, int linenr, Args ... args )
{
	char msgstr[32768];
	std::string format = messages[msg];
	memset(msgstr, 0, sizeof(msgstr));
	snprintf(msgstr, sizeof(msgstr) -1, format.c_str(), args ...);
	std::cout << msgstr;
}

#define MSG_OUT(msg, args...) msg_out(msg, __LINE__, ##args)
#define MSG_INFO(msg, args...) msg_info(msg, __LINE__, ##args)

#endif /* _MESSAGE_HELPER_H */
