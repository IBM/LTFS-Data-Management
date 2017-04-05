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
** FILE NAME:       Login.cc
**
** DESCRIPTION:     Login request message class of LTFS admin channel
**
** AUTHORS:         Atsushi Abe
**                  IBM Tokyo Lab., Japan
**                  piste@jp.ibm.com
**
*************************************************************************************
*/

#include "Login.h"

using namespace std;
using namespace ltfsadmin;

string Login::ToString()
{
	if (!doc_) {
		Prepare();
		if (user_.length() && password_.length()) {
			xmlNewTextChild(root_, NULL, BAD_CAST "user", BAD_CAST user_.c_str());
			xmlNewTextChild(root_, NULL, BAD_CAST "password", BAD_CAST password_.c_str());
		}
	}

	string ret = LTFSRequestMessage::ToString();

	return ret;
}
