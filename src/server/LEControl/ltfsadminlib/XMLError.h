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
** FILE NAME:       XMLError.h
**
** DESCRIPTION:     Error class for reporting when XML parsing or XML creation is failed
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

class XMLError : public AdminLibException {
public:
	XMLError(std::string file, int line,
			 std::string id, std::vector<std::string> args) throw()
		: AdminLibException(file, line, "XMLError", id, args) {};
	virtual ~XMLError() throw() {};

	virtual std::string GetOOBError();
	virtual const char* what(void);
};

}
