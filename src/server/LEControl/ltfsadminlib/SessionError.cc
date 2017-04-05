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
** FILE NAME:       SessionError.cc
**
** DESCRIPTION:     Admin Channel communication error class
**
** AUTHORS:         Atsushi Abe
**                  IBM Tokyo Lab., Japan
**                  piste@jp.ibm.com
**
*************************************************************************************
*/

#include <boost/format.hpp>

#include "SessionError.h"

using namespace ltfsadmin;

std::string SessionError::GetOOBError()
{
	return "ADMINLIB_SESSION";
}

const char* SessionError::what(void)
{
	boost::format msg_formatter("Session Error (%1%): [%2%:%3%] ");

	msg_formatter % id_;
	msg_formatter % file_;
	msg_formatter % line_;

	what_ = msg_formatter.str();

	return what_.c_str();
}
