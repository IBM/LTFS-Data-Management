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
** FILE NAME:       AdminLibException.h
**
** DESCRIPTION:     Base class of exception in ltfsadmin lib (virtual class)
**
** AUTHORS:         Atsushi Abe
**                  IBM Tokyo Lab., Japan
**                  piste@jp.ibm.com
**
*************************************************************************************
*/
#pragma once

#include <string>
#include <vector>
#include <exception>

namespace ltfsadmin {

class AdminLibException : public std::exception {
public:
	AdminLibException(std::string file, int line,
					  std::string type, std::string id,
					  std::vector<std::string> args) throw()
		: file_(file), line_(line), type_(type), id_(id), args_(args) {};

	virtual ~AdminLibException() throw() {};

	virtual std::string GetType() {return type_;};
	virtual std::string GetID() {return id_;};
	virtual std::string GetOOBError() = 0;
	virtual const char* what() = 0;

protected:
	std::string file_;
	int line_;
	std::string type_;
	std::string id_;
	std::vector<std::string> args_;
	std::string what_;
};

}
