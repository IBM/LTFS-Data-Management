#include <unordered_map>

#include "LTFSAdminLog.h"

std::unordered_map<std::string, std::string> ltfsadmin::LTFSAdminLog::msg_table_ = {
	{ std::string("100I"), std::string("Socket was terminated (%d)")},
	{ std::string("101I"), std::string("Socket error EINTR")},
	{ std::string("102I"), std::string("Socket error on recv (%d, %d)")}};
