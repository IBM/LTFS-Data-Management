#ifndef _MESSAGE_H
#define _MESSAGE_H

#include <string.h>
#include <string>
#include <fstream>
#include <iostream>

#include "src/common/msgcompiler/msgdefs.h"
#include "src/common/errors/errors.h"
#include "src/common/const/Const.h"

class Message {
private:
	std::ofstream messagefile;
public:
    Message();
	~Message();
	void writeOut(char *msgstr);
	void writeLog(char *msgstr);
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
	messageObject.writeOut(msgstr);
}

template<typename ... Args>
void msgInfo(msg_id msg, Args ... args )
{
	char msgstr[32768];
	std::string format = msgname[msg] + std::string("%s") + messages[msg];
	memset(msgstr, 0, sizeof(msgstr));
	snprintf(msgstr, sizeof(msgstr) -1, format.c_str(), "", args ...);
	messageObject.writeOut(msgstr);
}

template<typename ... Args>
void msgLog(msg_id msg, int linenr, Args ... args )
{
	char msgstr[32768];
	std::string format = msgname[msg] + std::string("(%d): ") + messages[msg];
	memset(msgstr, 0, sizeof(msgstr));
	snprintf(msgstr, sizeof(msgstr) -1, format.c_str(), linenr, args ...);
	messageObject.writeLog(msgstr);
}

#define MSG_OUT(msg, args...) msgOut(msg, __LINE__, ##args)
#define MSG_INFO(msg, args...) msgInfo(msg, ##args)
#define MSG_LOG(msg, args...) msgLog(msg, __LINE__, ##args)

#endif /* _MESSAGE_H */
