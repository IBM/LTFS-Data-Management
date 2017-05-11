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
