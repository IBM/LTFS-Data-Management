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

#include <unordered_map>

#include "LTFSResponseMessage.h"
#include "XMLError.h"

using namespace ltfsadmin;

LTFSResponseMessage::LTFSResponseMessage(const std::string& xml)
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

std::vector<xmlNode*> LTFSResponseMessage::GetNode(xmlNode* parent, const char* tag)
{
	std::vector<xmlNode*> nodes;

	for (xmlNode* cur = parent->children; cur; cur = cur->next) {
		if (cur->type == XML_ELEMENT_NODE && ! strcmp((const char*)cur->name, tag)) {
			Log(DEBUG3, "Found Node: " + std::string(tag));
			nodes.push_back(cur);
		}
	}

	return nodes;
}

std::unordered_map<std::string, std::string> LTFSResponseMessage::GetOptions(xmlNode* msg)
{
	std::unordered_map<std::string, std::string> ret;
	std::vector<xmlNode*> opts = GetNode(msg, "option");

	for (std::vector<xmlNode*>::iterator it = opts.begin(); it != opts.end(); ++it) {
		xmlChar* n = xmlGetProp(*it, BAD_CAST "name");
		if (n) {
			std::string name = std::string((const char*)n);
			xmlFree(n);

			xmlChar* t = xmlNodeGetContent(*it);
			if (t) {
				std::string content = std::string((const char*)t);
				ret[name] = content;
				xmlFree(t);
			} else {
				std::vector<std::string> args;
				args.push_back("Failed to get node content");
				throw XMLError(__FILE__, __LINE__, "041E", args);
			}
		} else {
			std::vector<std::string> args;
			args.push_back("Failed to get option name");
			throw XMLError(__FILE__, __LINE__, "042E", args);
		}
	}

	return ret;
}
