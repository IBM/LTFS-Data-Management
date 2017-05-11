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
