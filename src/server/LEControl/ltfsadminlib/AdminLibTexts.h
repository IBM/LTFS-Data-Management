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
** FILE NAME:       AdminLibTexts.h
**
** DESCRIPTION:     Error Descriptions
**
** AUTHORS:         Atsushi Abe
**                  IBM Tokyo Lab., Japan
**                  piste@jp.ibm.com
**
*************************************************************************************
*/
#pragma once

#include <string>

namespace ltfsadmin {

class AdminLibTexts {
public:
	AdminLibTexts()
	{
		messages_["001I"] = "";
	}

	virtual ~AdminLibTexts() {};

protected:
	std::unordered_map<string, string> messages_;
};

}
