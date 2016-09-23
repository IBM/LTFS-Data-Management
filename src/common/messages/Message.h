#ifndef _MESSAGE_H
#define _MESSAGE_H

#include <string.h>
#include <string>
#include <sstream>
#include <fstream>
#include <iostream>
#include <mutex>

#include "boost/format.hpp"

#include "msgdefs.h"
#include "src/common/errors/errors.h"
#include "src/common/const/Const.h"

class Message {
private:
	std::mutex mtx;
	std::ofstream messagefile;
public:
	enum LogType {
		STDOUT,
		LOGFILE
	};
private:
	std::atomic<Message::LogType> logType;

	inline void recurse(boost::format *fmter) {}
	template<typename T>
	void recurse(boost::format *fmter, T s)
	{
		*fmter % s;
	}
	template<typename T, typename ... Args>
	void recurse(boost::format *fmter, T s, Args ... args )
	{
		*fmter % s;
		recurse(fmter, args ...);
	}

	void writeOut(std::string msgstr);
	void writeLog(std::string msgstr);

	template<typename ... Args>
	void msgOut(msg_id msg, char *filename, int linenr, Args ... args )

	{
		std::string fmtstr = msgname[msg] + std::string("(%d): ") + messages[msg];
		boost::format fmter(fmtstr);
		fmter.exceptions( boost::io::all_error_bits );

		try {
			fmter % linenr;
			recurse(&fmter, args ...);
			writeOut(fmter.str());
		}
		catch(...) {
			std::cerr << messages[LTFSDMX0005E] << " (" << filename << ":" << linenr << ")" << std::endl;
			exit((int) LTFSDMErr::LTFSDM_GENERAL_ERROR);
		}
	}
	template<typename ... Args>
	void msgLog(msg_id msg, char *filename, int linenr,  Args ... args )
	{
		std::string fmtstr = msgname[msg] + std::string("(%d): ") + messages[msg];
		boost::format fmter(fmtstr);
		fmter.exceptions( boost::io::all_error_bits );

		try {
			fmter % linenr;
			recurse(&fmter, args ...);
			writeLog(fmter.str());
		}
		catch(...) {
			std::cerr << messages[LTFSDMX0005E] << " (" << filename << ":" << linenr << ")" << std::endl;
			exit((int) LTFSDMErr::LTFSDM_GENERAL_ERROR);
		}
	}

public:
    Message();
	~Message();

	void setLogType(Message::LogType type) {logType = type;}

	template<typename ... Args>
	void message(msg_id msg, char *filename, int linenr,  Args ... args )
	{
		if ( logType == Message::STDOUT )
			msgOut( msg, filename, linenr, args ...);
		else
			msgLog( msg, filename, linenr, args ...);
	}

	template<typename ... Args>
	void info(msg_id msg, char *filename, int linenr, Args ... args )
	{
		boost::format fmter(messages[msg]);
		fmter.exceptions( boost::io::all_error_bits );

		try {
			recurse(&fmter, args ...);
			writeOut(fmter.str());
		}
		catch(...) {
			std::cerr << messages[LTFSDMX0005E] << " (" << filename << ":" << linenr << ")" << std::endl;
			exit((int) LTFSDMErr::LTFSDM_GENERAL_ERROR);
		}
	}
};

extern Message messageObject;

#define MSG(msg, args ...) messageObject.message(msg, (char *) __FILE__, __LINE__, ##args)
#define INFO(msg, args ...) messageObject.info(msg, (char *) __FILE__, __LINE__, ##args)

#endif /* _MESSAGE_H */
