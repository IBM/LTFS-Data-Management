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
** FILE NAME:       Action.cc
**
** DESCRIPTION:     Action request message class of LTFS admin channel
**
** AUTHORS:         Atsushi Abe
**                  IBM Tokyo Lab., Japan
**                  piste@jp.ibm.com
**
*************************************************************************************
*/
#include "Action.h"
#include "InternalError.h"

using namespace std;
using namespace boost;
using namespace ltfsadmin;

string Action::ToString()
{
	if (!doc_) {
		Prepare();

		// Generate method string
		if (method_.length()) {
			xmlNewTextChild(root_, NULL, BAD_CAST "method", BAD_CAST method_.c_str());
		} else {
			vector<string> args;
			args.push_back("Cannot build action string: method not specified");
			throw InternalError(__FILE__, __LINE__, "060E", args);
			return "";
		}

		// Genarate target string
		string str_type;
		ltfs_object_t type = target_->GetObjectType();
		switch (type) {
			case LTFS_OBJ_CARTRIDGE:
				str_type = "cartridge";
				break;
			case LTFS_OBJ_DRIVE:
				str_type = "drive";
				break;
			case LTFS_OBJ_HISTORY:
			case LTFS_OBJ_JOB:
			case LTFS_OBJ_LIBRARY:
			case LTFS_OBJ_LTFS_NODE:
			case LTFS_OBJ_PRIVILEGE:
			default:
				vector<string> args;
				args.push_back("Cannot build action string because of unexpected type");
				throw InternalError(__FILE__, __LINE__, "061E", args);
				return "";
				break;
		}

		if (target_) {
			xmlNode* target = xmlNewTextChild(root_, NULL, BAD_CAST "target", BAD_CAST "");
			xmlNode* object = xmlNewTextChild(target, NULL, BAD_CAST "object", BAD_CAST "");
			xmlNewNsProp(object, NULL, BAD_CAST "type", BAD_CAST str_type.c_str());
			xmlNewNsProp(object, NULL, BAD_CAST "id", BAD_CAST target_->GetObjectID().c_str());
		} else {
			vector<string> args;
			args.push_back("Cannot build action string: cannot get object ID");
			throw InternalError(__FILE__, __LINE__, "062E", args);
			return "";
		}

		// Handle options here
		if (options_.size()) {
			for (unordered_map<string, string>::iterator it = options_.begin(); it != options_.end(); ++it) {
				xmlNode* opt_node = xmlNewTextChild(root_, NULL, BAD_CAST "option", BAD_CAST it->second.c_str());
				xmlNewNsProp(opt_node, NULL, BAD_CAST "name", BAD_CAST it->first.c_str());
			}
		}
	}

	string ret = LTFSRequestMessage::ToString();

	return ret;
}
