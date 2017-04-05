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
** FILE NAME:       Inventory.h
**
** DESCRIPTION:     Inventory request message class of LTFS admin channel
**
** AUTHORS:         Atsushi Abe
**                  IBM Tokyo Lab., Japan
**                  piste@jp.ibm.com
**
*************************************************************************************
*/
#pragma once

#include <string>

//#include <boost/unordered_map.hpp>

#include "LTFSRequestMessage.h"

namespace ltfsadmin {

class Inventory:public LTFSRequestMessage {
public:
	Inventory() :
		LTFSRequestMessage(LTFS_MSG_INVENTORY, 0) , object_type_("") {};
	Inventory(uint64_t sequence, std::string object_type, std::unordered_map<std::string, std::string> options ) :
		LTFSRequestMessage(LTFS_MSG_INVENTORY, sequence) , object_type_(object_type), options_(options) {};
	virtual ~Inventory() {};
	virtual std::string ToString();

private:
	std::string object_type_;
	std::unordered_map<std::string, std::string> options_;
};

}
