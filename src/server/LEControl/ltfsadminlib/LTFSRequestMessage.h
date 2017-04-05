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
** FILE NAME:       LTFSRequestMessage.h
**
** DESCRIPTION:     Request message class of  LTFS admin channel
**
** AUTHORS:         Atsushi Abe
**                  IBM Tokyo Lab., Japan
**                  piste@jp.ibm.com
**
*************************************************************************************
*/
#pragma once

#include <string>

#include "LTFSAdminMessage.h"

namespace ltfsadmin {

class LTFSRequestMessage:public LTFSAdminMessage {
public:
	LTFSRequestMessage() {};
	LTFSRequestMessage(ltfs_message_t type, uint64_t seq) : LTFSAdminMessage(type, seq) { };
	virtual ~LTFSRequestMessage() {};

protected:
	virtual void Parse(void) {}; /* Parse function is not needed to request message classes */
};

}
