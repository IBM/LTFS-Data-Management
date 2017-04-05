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
** FILE NAME:       InternalError.h
**
** DESCRIPTION:     Error class for reporting when internal logic error is detected
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

class InternalError : public AdminLibException {
public:
	InternalError(std::string file, int line,
				 std::string id, std::vector<std::string> args) throw()
		: AdminLibException(file, line, "InternalError", id, args) {};
	virtual ~InternalError() throw() {};

	virtual std::string GetOOBError();
	virtual const char* what(void);
};

}
