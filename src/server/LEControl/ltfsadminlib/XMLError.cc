#include <boost/format.hpp>

#include "XMLError.h"

using namespace ltfsadmin;

std::string XMLError::GetOOBError()
{
	return "ADMINLIB_XML";
}

const char* XMLError::what(void)
{
	boost::format msg_formatter("XML Error (%1%): [%2%:%3%] ");

	msg_formatter % id_;
	msg_formatter % file_;
	msg_formatter % line_;

	what_ = msg_formatter.str();

	return what_.c_str();
}
