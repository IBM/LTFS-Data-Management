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
** FILE NAME:       Inventory.cc
**
** DESCRIPTION:     Inventory request message class of LTFS admin channel
**
** AUTHORS:         Atsushi Abe
**                  IBM Tokyo Lab., Japan
**                  piste@jp.ibm.com
**
*************************************************************************************
*/
#include "Inventory.h"

using namespace std;
using namespace boost;
using namespace ltfsadmin;

string Inventory::ToString()
{
	if (!doc_) {
		Prepare();
		if (object_type_.length()) {
			xmlNewTextChild(root_, NULL, BAD_CAST "object_type", BAD_CAST object_type_.c_str());
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
