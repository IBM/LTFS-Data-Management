#ifndef _MESSAGE_H
#define _MESSAGE_H

#include <string.h>
#include <string>
#include <sstream>
#include <fstream>
#include <iostream>

#include "boost/format.hpp"

#include "msgdefs.h"
#include "src/common/errors/errors.h"
#include "src/common/const/Const.h"

class Message {
private:
	std::mutex mtx;
	std::ofstream messagefile;

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

public:
    Message();
	~Message();

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
			std::cerr << messages[OLTFSX0005E] << " (" << filename << ":" << linenr << ")" << std::endl;
			exit((int) OLTFSErr::OLTFS_GENERAL_ERROR);
		}
	}

	template<typename ... Args>
	void msgInfo(msg_id msg, char *filename, int linenr, Args ... args )
	{
		boost::format fmter(messages[msg]);
		fmter.exceptions( boost::io::all_error_bits );

		try {
			recurse(&fmter, args ...);
			writeOut(fmter.str());
		}
		catch(...) {
			std::cerr << messages[OLTFSX0005E] << " (" << filename << ":" << linenr << ")" << std::endl;
			exit((int) OLTFSErr::OLTFS_GENERAL_ERROR);
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
			std::cerr << messages[OLTFSX0005E] << " (" << filename << ":" << linenr << ")" << std::endl;
			exit((int) OLTFSErr::OLTFS_GENERAL_ERROR);
		}
	}
};

extern Message messageObject;

#define MSG_OUT(msg, args ...) messageObject.msgOut(msg, (char *) __FILE__, __LINE__, ##args)
#define MSG_INFO(msg, args ...) messageObject.msgInfo(msg, (char *) __FILE__, __LINE__, ##args)
#define MSG_LOG(msg, args ...) messageObject.msgLog(msg, (char *) __FILE__, __LINE__, ##args)

#endif /* _MESSAGE_H */
