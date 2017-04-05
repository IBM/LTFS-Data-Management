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

#include <string>
#include <cstring>

#include "LTFSResponseMessage.h"
#include "XMLError.h"

using namespace std;
using namespace boost;
using namespace ltfsadmin;

LTFSResponseMessage::LTFSResponseMessage(const string& xml)
	: LTFSAdminMessage(xml)
{
}

uint64_t LTFSResponseMessage::GetSequence(void)
{
	if (!root_)
		Parse();

	return sequence_;
}

ltfs_message_t LTFSResponseMessage::GetType()
{
	if (!root_)
		Parse();

	return type_;
}

void LTFSResponseMessage::Parse(void)
{
	LTFSAdminMessage::Parse();
}

vector<xmlNode*> LTFSResponseMessage::GetNode(xmlNode* parent, const char* tag)
{
	vector<xmlNode*> nodes;

	for (xmlNode* cur = parent->children; cur; cur = cur->next) {
		if (cur->type == XML_ELEMENT_NODE && ! strcmp((const char*)cur->name, tag)) {
			Log(DEBUG3, "Found Node: " + string(tag));
			nodes.push_back(cur);
		}
	}

	return nodes;
}

unordered_map<string, string> LTFSResponseMessage::GetOptions(xmlNode* msg)
{
	unordered_map<string, string> ret;
	vector<xmlNode*> opts = GetNode(msg, "option");

	for (vector<xmlNode*>::iterator it = opts.begin(); it != opts.end(); ++it) {
		xmlChar* n = xmlGetProp(*it, BAD_CAST "name");
		if (n) {
			string name = string((const char*)n);
			xmlFree(n);

			xmlChar* t = xmlNodeGetContent(*it);
			if (t) {
				string content = string((const char*)t);
				ret[name] = content;
				xmlFree(t);
			} else {
				vector<string> args;
				args.push_back("Failed to get node content");
				throw XMLError(__FILE__, __LINE__, "041E", args);
			}
		} else {
			vector<string> args;
			args.push_back("Failed to get option name");
			throw XMLError(__FILE__, __LINE__, "042E", args);
		}
	}

	return ret;
}
