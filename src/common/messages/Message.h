#ifndef _MESSAGE_H
#define _MESSAGE_H

#include <string.h>
#include <string>
#include <fstream>
#include <iostream>

#include "src/common/msgcompiler/msgdefs.h"
#include "src/common/errors/errors.h"
#include "src/common/const/Const.h"

#define MSG_INTERN(msg, args...) msgOut(msg, __LINE__, ##args)

class Message {
private:
	std::ofstream messagefile;
	bool toFile;
public:
    Message() : toFile(false) {};
	~Message();
	void write(char *msgstr);
	void redirectToFile();
};

extern Message messageObject;

template<typename ... Args>
void msgOut(msg_id msg, int linenr, Args ... args )
{
	char msgstr[32768];
	std::string format = msgname[msg] + std::string("(%d): ") + messages[msg];
	memset(msgstr, 0, sizeof(msgstr));
	snprintf(msgstr, sizeof(msgstr) -1, format.c_str(), linenr, args ...);
	messageObject.write(msgstr);
}

template<typename ... Args>
void msgInfo(msg_id msg, Args ... args )
{
	char msgstr[32768];
	std::string format = messages[msg];
	memset(msgstr, 0, sizeof(msgstr));
	snprintf(msgstr, sizeof(msgstr) -1, format.c_str(), args ...);
	messageObject.write(msgstr);
}

#define MSG_OUT(msg, args...) msgOut(msg, __LINE__, ##args)
#define MSG_INFO(msg, args...) msgInfo(msg, ##args)

#endif /* _MESSAGE_H */
