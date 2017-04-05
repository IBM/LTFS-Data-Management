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

#include <boost/assign.hpp>

#include "LTFSAdminLog.h"

using namespace std;
using namespace boost;

unordered_map<string, string> ltfsadmin::LTFSAdminLog::msg_table_ = boost::assign::map_list_of
	( string("100I"), string("Socket was terminated (%d)"))
	( string("101I"), string("Socket error EINTR"))
	( string("102I"), string("Socket error on recv (%d, %d)"))
	;
