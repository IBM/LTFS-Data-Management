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
