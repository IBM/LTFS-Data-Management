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
** FILE NAME:       RequestError.h
**
** DESCRIPTION:     Error class for reporting when admin request is failed
**
** AUTHORS:         Atsushi Abe
**                  IBM Tokyo Lab., Japan
**                  piste@jp.ibm.com
**
*************************************************************************************
*/
#pragma once

#include <string>

#include "AdminLibException.h"

namespace ltfsadmin {

class RequestError : public AdminLibException {
public:
	RequestError(std::string file, int line,
				 std::string id, std::vector<std::string> args) throw()
		: AdminLibException(file, line, "RequestError", id, args) {};
	virtual ~RequestError() throw() {};

	virtual std::string GetOOBError();
	virtual const char* what(void);
};

}
