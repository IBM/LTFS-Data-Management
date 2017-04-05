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
** FILE NAME:       XMLError.cc
**
** DESCRIPTION:     Error class for reporting when XML parsing or XML creation is failed
**
** AUTHORS:         Atsushi Abe
**                  IBM Tokyo Lab., Japan
**                  piste@jp.ibm.com
**
*************************************************************************************
*/

#include <boost/format.hpp>

#include "XMLError.h"

using namespace std;
using namespace boost;
using namespace ltfsadmin;

string XMLError::GetOOBError()
{
	return "ADMINLIB_XML";
}

const char* XMLError::what(void)
{
	format msg_formatter("XML Error (%1%): [%2%:%3%] ");

	msg_formatter % id_;
	msg_formatter % file_;
	msg_formatter % line_;

	what_ = msg_formatter.str();

	return what_.c_str();
}
