/*
**  %Z% %I% %W% %G% %U%
**
**  ZZ_Copyright_BEGIN
**  ZZ_Copyright_END
**
*************************************************************************************
**
** COMPONENT NAME:  IBM Linear Tape File System
**
** FILE NAME:       RequestError.cc
**
** DESCRIPTION:     Error class for reporting when admin request is failed
**
** AUTHORS:         Atsushi Abe
**                  IBM Tokyo Lab., Japan
**                  piste@jp.ibm.com
**
*************************************************************************************
*/

#include <boost/format.hpp>
#include <boost/algorithm/string.hpp>

#include "RequestError.h"

using namespace std;
using namespace boost;
using namespace ltfsadmin;

string RequestError::GetOOBError()
{
	if (args_.size() < 2)
		return "ADMINLIB_REQUEST";

	vector<string> msgs;

	boost::algorithm::split(msgs, args_[1], boost::is_any_of(" "));
	if (msgs.size() < 2) {
		return "ADMINLIB_REQUEST";
	} else {
		return msgs[1];
	}
}

const char* RequestError::what(void)
{
	format msg_formatter("Request Error (%1%): [%2%:%3%]%4%");

	msg_formatter % id_;
	msg_formatter % file_;
	msg_formatter % line_;

	string arg;
	for (unsigned int i = 0; i < args_.size(); ++i) {
		arg = arg + ": " + args_[i];
	}
	msg_formatter % arg;

	what_ = msg_formatter.str();

	return what_.c_str();
}
