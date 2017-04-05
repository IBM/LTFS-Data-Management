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
** FILE NAME:       LTFSAdminMessage.cc
**
** DESCRIPTION:     Base class for LTFS admin channel message
**
** AUTHORS:         Atsushi Abe
**                  IBM Tokyo Lab., Japan
**                  piste@jp.ibm.com
**
*************************************************************************************
*/

#include <sstream>
#include <iostream>
#include <string>
#include <cstring>

#include "LTFSAdminMessage.h"
#include "XMLError.h"

#define ERROR_PARSE_NO_VERSION_TAG (1)
#define ERROR_PARSE_NO_SEQ_TAG     (2)
#define ERROR_PARSE_NO_TYPE_TAG    (3)
#define ERROR_PARSE_WRONG_TYPE     (4)
#define ERROR_PARSE_WRONG_MESSAGE  (5)

using namespace std;
using namespace ltfsadmin;

LTFSAdminMessage::LTFSAdminMessage()
	: version_(SUPPORTED_ADMIN_PROTOCOL_VERSION), sequence_(0), type_(LTFS_MSG_UNKNOWN), xml_(""), doc_(NULL), root_(NULL)
{
}

LTFSAdminMessage::LTFSAdminMessage(ltfs_message_t type, uint64_t seq)
	: version_(SUPPORTED_ADMIN_PROTOCOL_VERSION), sequence_(seq), type_(type), xml_(""), doc_(NULL), root_(NULL)
{
}

LTFSAdminMessage::LTFSAdminMessage(const string& xml)
	: version_(""), sequence_(0), type_(LTFS_MSG_UNKNOWN), xml_(xml), doc_(NULL), root_(NULL)
{
}

LTFSAdminMessage::~LTFSAdminMessage()
{
	if (doc_ != NULL) {
		Log(DEBUG2, "Free XML document");
		xmlFreeDoc(doc_);
		doc_ = NULL;
	}
}

void LTFSAdminMessage::Parse(void)
{
	if (!doc_) {
		Log(DEBUG2, "Parsing the XML document");
		doc_ = xmlReadMemory(xml_.c_str(), xml_.length(), NULL, NULL,
								   XML_PARSE_NOERROR | XML_PARSE_NOWARNING);
		if (! doc_) {
			vector<string> args;
			args.push_back("Cannot create XML document");
			throw XMLError(__FILE__, __LINE__, "020E", args);
		}
		Log(DEBUG2, "Parsed the XML document");
	}

	if (!root_) {
		Log(DEBUG2, "Getting the root element");
		root_ = xmlDocGetRootElement(doc_);
		if (! root_) {
			vector<string> args;
			args.push_back("Cannot get root element of XML docuemnt");
			throw XMLError(__FILE__, __LINE__, "021E", args);
		}
		Log(DEBUG2, "Got the root element");
	}

	try {
		if (! strcmp((const char*)root_->name, "ltfsadmin_message")) {
			xmlChar *val = xmlGetProp(root_, BAD_CAST "version");
			if (val) {
				version_ = string((const char*)val);
				xmlFree(val);
			} else
				throw ERROR_PARSE_NO_VERSION_TAG;

			val = xmlGetProp(root_, BAD_CAST "sequence");
			if (val) {
				string v = string((const char*)val);
				istringstream iss(v);
				if (! ((iss) >> sequence_)) {
					xmlFree(val);
					throw ERROR_PARSE_NO_SEQ_TAG;
				}
				xmlFree(val);
			} else
				throw ERROR_PARSE_NO_SEQ_TAG;

			val = xmlGetProp(root_, BAD_CAST "type");
			if (val) {
				// TODO: Add more message types
				if (!strcmp((const char*) val, "identifier"))
					type_ = LTFS_MSG_IDENTIFIER;
				else if (!strcmp((const char*)val, "result"))
					type_ = LTFS_MSG_RESULT;
				else {
					xmlFree(val);
					throw ERROR_PARSE_WRONG_TYPE;
				}
				xmlFree(val);
			} else
				throw ERROR_PARSE_NO_TYPE_TAG;
		} else
			throw ERROR_PARSE_WRONG_MESSAGE;
	} catch (int err) {
		vector<string> args;
		args.push_back("Cannot parse LTFSAdminMessage correctly");
		switch (err) {
			case ERROR_PARSE_NO_VERSION_TAG:
				args.push_back("Version tag is not found");
				break;
			case ERROR_PARSE_NO_SEQ_TAG:
				args.push_back("Sequence tag is not found");
				break;
			case ERROR_PARSE_WRONG_TYPE:
				args.push_back("Unexpected type is found");
				break;
			case ERROR_PARSE_WRONG_MESSAGE:
				args.push_back("Unexpected message is detected");
				break;
			default:
				args.push_back("Unexpected error");
				break;
		}
		throw XMLError(__FILE__, __LINE__, "022E", args);
	}
}

void LTFSAdminMessage::Prepare(void)
{
	if (! doc_) {
		char buf[32]; /* 19 byte buffer is required to store MAX_UINT64 in char buffer at basded 10 */
		doc_  = xmlNewDoc(BAD_CAST "1.0");
		root_ = xmlNewNode(NULL, BAD_CAST "ltfsadmin_message");
		xmlDocSetRootElement(doc_, root_);
		xmlNewProp(root_, BAD_CAST "version", BAD_CAST version_.c_str());
		snprintf(buf, sizeof(buf), "%llu", (unsigned long long)sequence_);
		xmlNewProp(root_, BAD_CAST "sequence", BAD_CAST buf);
		switch(type_) {
			case LTFS_MSG_INVENTORY:
				xmlNewProp(root_, BAD_CAST "type", BAD_CAST "inventory");
				break;
			case LTFS_MSG_ACTION:
				xmlNewProp(root_, BAD_CAST "type", BAD_CAST "action");
				break;
			case LTFS_MSG_LOGIN:
				xmlNewProp(root_, BAD_CAST "type", BAD_CAST "login");
				break;
			case LTFS_MSG_LOGOUT:
				xmlNewProp(root_, BAD_CAST "type", BAD_CAST "logout");
				break;
			default:
				Log(DEBUG2, "Unable to create LTFSAdminMessage");
				vector<string> args;
				args.push_back("Cannot build LTFSAdminMessage correctly");
				throw XMLError(__FILE__, __LINE__, "023E", args);
				break;
		}
		Log(DEBUG2, "Created LTFSAdminMessage");
	}

	return;
}

#include <string.h>
string LTFSAdminMessage::ToString()
{
	xmlChar* xml;
	int      xml_size;

	if (!doc_)
		Prepare();

	if (!xml_.length()) {
		// Output xml to member variables xml_
		xmlDocDumpMemoryEnc(doc_, &xml, &xml_size, "UTF-8");
		xml_ = string((const char*)xml, xml_size);
		xmlFree(xml);
		Log(DEBUG2, "Built LTFSAdminMessage as XML");
	}

	return xml_;
}
