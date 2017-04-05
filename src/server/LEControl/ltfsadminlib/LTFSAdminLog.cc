/*
**  %Z% %I% %W% %G% %U%
**
**  ZZ_Copyright_BEGIN
**
**
**  IBM Confidential
**
**  OCO Source Materials
**
**  IBM Linear Tape File System Enterprise Edition
**
**  (C) Copyright IBM Corp. 2015
**
**  The source code for this program is not published or other-
**  wise divested of its trade secrets, irrespective of what has
**  been deposited with the U.S. Copyright Office
**
**
**  ZZ_Copyright_END
*/

#include <unordered_map>

#include "LTFSAdminLog.h"

std::unordered_map<std::string, std::string> ltfsadmin::LTFSAdminLog::msg_table_ = {
	{ std::string("100I"), std::string("Socket was terminated (%d)")},
	{ std::string("101I"), std::string("Socket error EINTR")},
	{ std::string("102I"), std::string("Socket error on recv (%d, %d)")}};
