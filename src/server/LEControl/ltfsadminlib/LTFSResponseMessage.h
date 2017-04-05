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
** FILE NAME:       LTFSResponseMessage.h
**
** DESCRIPTION:     Response message class of LTFS admin channel
**
** AUTHORS:         Atsushi Abe
**                  IBM Tokyo Lab., Japan
**                  piste@jp.ibm.com
**
*************************************************************************************
*/
#pragma once

#include <string>
#include <boost/shared_ptr.hpp>
#include <boost/unordered_map.hpp>

#include "LTFSAdminMessage.h"

namespace ltfsadmin {

class LTFSResponseMessage:public LTFSAdminMessage {
public:
	LTFSResponseMessage() {};
	LTFSResponseMessage(const std::string& xml);
	virtual ~LTFSResponseMessage() {};
	virtual std::string ToString() {return xml_;}; // Override-final
	virtual uint64_t GetSequence(void);  // Override-final
	virtual ltfs_message_t GetType();    // Override-final

protected:
	virtual void Parse(void);
	virtual void Prepare(void) {}; // Prepare method is not needed to response message classes */
	std::vector<xmlNode*> GetNode(xmlNode* parent, const char* tag);
	boost::unordered_map<std::string, std::string> GetOptions(xmlNode* msg);
};

}
