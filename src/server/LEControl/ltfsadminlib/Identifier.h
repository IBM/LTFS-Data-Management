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
** FILE NAME:       Identifier.h
**
** DESCRIPTION:     Identifier request message class of LTFS admin channel
**
** AUTHORS:         Atsushi Abe
**                  IBM Tokyo Lab., Japan
**                  piste@jp.ibm.com
**
*************************************************************************************
*/
#pragma once

#include <string>

#include "LTFSResponseMessage.h"

namespace ltfsadmin {

class Identifier:public LTFSResponseMessage {
public:
	Identifier(std::string& xml);
	virtual ~Identifier(){};
	bool GetAuth();

private:
	bool auth_;
	virtual void Parse(void);
};

}
