#pragma once

#include <string>

#include "AdminLibException.h"

namespace ltfsadmin {

class SessionError : public AdminLibException {
public:
	SessionError(std::string file, int line,
				 std::string id, std::vector<std::string> args) throw()
		: AdminLibException(file, line, "SessionError", id, args) {};
	virtual ~SessionError() throw() {};

	virtual std::string GetOOBError();
	virtual const char* what(void);
};

}
