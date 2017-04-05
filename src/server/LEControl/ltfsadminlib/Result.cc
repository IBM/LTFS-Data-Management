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
** FILE NAME:       Result.cc
**
** DESCRIPTION:     Result message class of LTFS admin channel
**
** AUTHORS:         Atsushi Abe
**                  IBM Tokyo Lab., Japan
**                  piste@jp.ibm.com
**
*************************************************************************************
*/
#include <iostream>
#include <vector>
#include <cstring>

#include <unordered_map>
#include <memory>
#include <list>

#include "Result.h"
#include "XMLError.h"

#include "LTFSNode.h"
#include "Drive.h"
#include "Cartridge.h"

using namespace ltfsadmin;

Result::Result(std::string& xml, LTFSAdminSession *session)
	: LTFSResponseMessage(xml), session_(session)
{
};

ltfs_status_t Result::GetStatus(void)
{
	if (!root_)
		Parse();

	return status_;
}

std::string Result::GetOutput(void)
{
	if (!root_)
		Parse();

	return output_;
}

std::list<std::shared_ptr <LTFSObject> >& Result::GetObjects()
{
	if (!root_)
		Parse();

	if (! root_) {
		std::vector<std::string> args;
		args.push_back("Cannot parse object");
		throw XMLError(__FILE__, __LINE__, "050E", args);
	}

	return objects_;
}

std::list<std::shared_ptr <LTFSNode> >& Result::GetLTFSNodes()
{
	if (!root_)
		Parse();

	if (! root_) {
		std::vector<std::string> args;
		args.push_back("Cannot parse object");
		throw XMLError(__FILE__, __LINE__, "050E", args);
	}

	return nodes_;
}

std::list<std::shared_ptr <Drive> >& Result::GetDrives()
{
	if (!root_)
		Parse();

	if (! root_) {
		std::vector<std::string> args;
		args.push_back("Cannot parse object");
		throw XMLError(__FILE__, __LINE__, "050E", args);
	}

	return drives_;
}

std::list<std::shared_ptr <Cartridge> >& Result::GetCartridges()
{
	if (!root_)
		Parse();

	if (! root_) {
		std::vector<std::string> args;
		args.push_back("Cannot parse object");
		throw XMLError(__FILE__, __LINE__, "050E", args);
	}

	return cartridges_;
}

std::unordered_map<std::string, std::string> Result::GetObjectAttributes(xmlNode* obj)
{
	std::unordered_map<std::string, std::string> ret;
	std::vector<xmlNode*> attrs = GetNode(obj, "attribute");

	for (std::vector<xmlNode*>::iterator it = attrs.begin(); it != attrs.end(); ++it) {
		xmlChar* n = xmlGetProp(*it, BAD_CAST "name");
		if (n) {
			std::string name = std::string((const char*)n);
			xmlFree(n);

			std::vector<xmlNode*> v = GetNode(*it, "value");
			if (v.size() == 1) {
				xmlChar* t = xmlNodeGetContent(v[0]);
				if (t) {
					std::string content = std::string((const char*)t);
					ret[name] = content;
					xmlFree(t);
				} else {
					std::vector<std::string> args;
					args.push_back("Failed to get attribute value");
					throw XMLError(__FILE__, __LINE__, "051E", args);
				}
			} else {
				std::vector<std::string> args;
				args.push_back("Failed to get correct attribute value");
				throw XMLError(__FILE__, __LINE__, "052E", args);
			}
		} else {
				std::vector<std::string> args;
				args.push_back("Failed to get attribute name");
				throw XMLError(__FILE__, __LINE__, "053E", args);
		}
	}

	return ret;
}

std::string Result::GetObjectType(xmlNode* obj)
{
	std::string type = "";
	xmlChar* t = xmlGetProp(obj, BAD_CAST "type");

	if (t) {
		type = std::string((const char*)t);
		xmlFree(t);
	} else {
		std::vector<std::string> args;
		args.push_back("Cannot get object type");
		throw XMLError(__FILE__, __LINE__, "054E", args);
	}

	return type;
}

std::string Result::GetObjectID(xmlNode* obj)
{
	std::string id = "";
	xmlChar* i = xmlGetProp(obj, BAD_CAST "id");

	if (i) {
		id = std::string((const char*)i);
		xmlFree(i);
	} else {
		std::vector<std::string> args;
		args.push_back("Cannot get object ID");
		throw XMLError(__FILE__, __LINE__, "055E", args);
	}

	return id;
}

void Result::Parse(void)
{
	if (!root_)
		LTFSResponseMessage::Parse();

	if (root_) {
		/* Parse status from received XML */
		std::vector<xmlNode*> statuses = GetNode(root_, "status");
		std::vector<xmlNode*> outputs = GetNode(root_, "output");
		std::vector<std::string> args;

		switch (statuses.size()) {
			case 1:
				// Normal case fall through
				break;
			case 0:
				args.push_back("No status is found in Result Message");
				throw XMLError(__FILE__, __LINE__, "056E", args);
				break;
			default:
				args.push_back("Multiple statuses are found in Result Message");
				throw XMLError(__FILE__, __LINE__, "057E", args);
				break;
		}

		xmlChar* t = xmlNodeGetContent(statuses[0]);
		if (! strcmp((const char*)t, "success"))
			status_ = LTFS_ADMIN_SUCCESS;
		else if (! strcmp((const char*)t, "error"))
			status_ = LTFS_ADMIN_ERROR;
		else if (! strcmp((const char*)t, "disconnected"))
			status_ = LTFS_ADMIN_DISCONNECTED;
		else
			status_ = LTFS_ADMIN_UNKNOWN;
		xmlFree(t);

		for (unsigned int i = 0; i < outputs.size(); ++i) {
			xmlChar* o = xmlNodeGetContent(outputs[i]);
			if (i)
				output_ = output_ + " " + std::string((const char*)o);
			else
				output_ = std::string((const char*)o);
			xmlFree(o);
		}
		Log(DEBUG2, "Output: " + output_);

		/* Parse Objects here if required */
		std::vector<xmlNode*> object_root = GetNode(root_, "objects");
		if (object_root.size() > 1) {
			std::vector<std::string> args;
			args.push_back("Multiple object tags are found");
			throw XMLError(__FILE__, __LINE__, "058E", args);
		} else if (object_root.size() == 1) {
			std::vector<xmlNode*> objects = GetNode(object_root[0], "object");
			for (std::vector<xmlNode*>::iterator it = objects.begin(); it != objects.end(); ++it) {
				std::string type = GetObjectType(*it);
				std::string id = GetObjectID(*it);
				std::unordered_map<std::string, std::string> elems = GetObjectAttributes(*it);

				/* Ignore 0-byte ID objects */
				if (id.size() == 0)
					continue;

				Log(DEBUG2, "Found a object: type = " + type + ", id =" + id);
				elems["id"] = id;

				std::shared_ptr<LTFSObject> obj;
				if (type == "LTFSNode") {
					std::shared_ptr<LTFSNode> node = std::shared_ptr<LTFSNode>(new LTFSNode(elems, session_));
					nodes_.push_back(node);
					obj = std::shared_ptr<LTFSObject>(node);
				} else if (type == "cartridge") {
					std::shared_ptr<Cartridge> cart = std::shared_ptr<Cartridge>(new Cartridge(elems, session_));
					cartridges_.push_back(cart);
					obj = std::shared_ptr<LTFSObject>(cart);
				} else if (type == "drive") {
					std::shared_ptr<Drive> drive = std::shared_ptr<Drive>(new Drive(elems, session_));
					drives_.push_back(drive);
					obj = std::shared_ptr<LTFSObject>(drive);
				}

				objects_.push_back(obj);
			}
		}
	} else {
		std::vector<std::string> args;
		args.push_back("Document root element is not found");
		throw XMLError(__FILE__, __LINE__, "059E", args);
	}
}
