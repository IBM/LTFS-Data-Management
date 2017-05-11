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
