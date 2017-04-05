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
** FILE NAME:       InternalError.cc
**
** DESCRIPTION:     Error class for reporting when internal logic error is detected
**
** AUTHORS:         Atsushi Abe
**                  IBM Tokyo Lab., Japan
**                  piste@jp.ibm.com
**
*************************************************************************************
*/

#include <boost/format.hpp>

#include "InternalError.h"

using namespace ltfsadmin;

std::string InternalError::GetOOBError()
{
	return "ADMINLIB_INTERNAL";
}

const char* InternalError::what(void)
{
	boost::format msg_formatter("Internal Error (%1%): [%2%:%3%] ");

	msg_formatter % id_;
	msg_formatter % file_;
	msg_formatter % line_;

	what_ = msg_formatter.str();

	return what_.c_str();
}
