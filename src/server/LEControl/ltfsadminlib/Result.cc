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

#include "Result.h"
#include "XMLError.h"

#include "LTFSNode.h"
#include "Drive.h"
#include "Cartridge.h"

using namespace std;
using namespace boost;
using namespace ltfsadmin;

Result::Result(string& xml, LTFSAdminSession *session)
	: LTFSResponseMessage(xml), session_(session)
{
};

ltfs_status_t Result::GetStatus(void)
{
	if (!root_)
		Parse();

	return status_;
}

string Result::GetOutput(void)
{
	if (!root_)
		Parse();

	return output_;
}

list<shared_ptr <LTFSObject> >& Result::GetObjects()
{
	if (!root_)
		Parse();

	if (! root_) {
		vector<string> args;
		args.push_back("Cannot parse object");
		throw XMLError(__FILE__, __LINE__, "050E", args);
	}

	return objects_;
}

list<shared_ptr <LTFSNode> >& Result::GetLTFSNodes()
{
	if (!root_)
		Parse();

	if (! root_) {
		vector<string> args;
		args.push_back("Cannot parse object");
		throw XMLError(__FILE__, __LINE__, "050E", args);
	}

	return nodes_;
}

list<shared_ptr <Drive> >& Result::GetDrives()
{
	if (!root_)
		Parse();

	if (! root_) {
		vector<string> args;
		args.push_back("Cannot parse object");
		throw XMLError(__FILE__, __LINE__, "050E", args);
	}

	return drives_;
}

list<shared_ptr <Cartridge> >& Result::GetCartridges()
{
	if (!root_)
		Parse();

	if (! root_) {
		vector<string> args;
		args.push_back("Cannot parse object");
		throw XMLError(__FILE__, __LINE__, "050E", args);
	}

	return cartridges_;
}

unordered_map<string, string> Result::GetObjectAttributes(xmlNode* obj)
{
	unordered_map<string, string> ret;
	vector<xmlNode*> attrs = GetNode(obj, "attribute");

	for (vector<xmlNode*>::iterator it = attrs.begin(); it != attrs.end(); ++it) {
		xmlChar* n = xmlGetProp(*it, BAD_CAST "name");
		if (n) {
			string name = string((const char*)n);
			xmlFree(n);

			vector<xmlNode*> v = GetNode(*it, "value");
			if (v.size() == 1) {
				xmlChar* t = xmlNodeGetContent(v[0]);
				if (t) {
					string content = string((const char*)t);
					ret[name] = content;
					xmlFree(t);
				} else {
					vector<string> args;
					args.push_back("Failed to get attribute value");
					throw XMLError(__FILE__, __LINE__, "051E", args);
				}
			} else {
				vector<string> args;
				args.push_back("Failed to get correct attribute value");
				throw XMLError(__FILE__, __LINE__, "052E", args);
			}
		} else {
				vector<string> args;
				args.push_back("Failed to get attribute name");
				throw XMLError(__FILE__, __LINE__, "053E", args);
		}
	}

	return ret;
}

string Result::GetObjectType(xmlNode* obj)
{
	string type = "";
	xmlChar* t = xmlGetProp(obj, BAD_CAST "type");

	if (t) {
		type = string((const char*)t);
		xmlFree(t);
	} else {
		vector<string> args;
		args.push_back("Cannot get object type");
		throw XMLError(__FILE__, __LINE__, "054E", args);
	}

	return type;
}

string Result::GetObjectID(xmlNode* obj)
{
	string id = "";
	xmlChar* i = xmlGetProp(obj, BAD_CAST "id");

	if (i) {
		id = string((const char*)i);
		xmlFree(i);
	} else {
		vector<string> args;
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
		vector<xmlNode*> statuses = GetNode(root_, "status");
		vector<xmlNode*> outputs = GetNode(root_, "output");
		vector<string> args;

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
				output_ = output_ + " " + string((const char*)o);
			else
				output_ = string((const char*)o);
			xmlFree(o);
		}
		Log(DEBUG2, "Output: " + output_);

		/* Parse Objects here if required */
		vector<xmlNode*> object_root = GetNode(root_, "objects");
		if (object_root.size() > 1) {
			vector<string> args;
			args.push_back("Multiple object tags are found");
			throw XMLError(__FILE__, __LINE__, "058E", args);
		} else if (object_root.size() == 1) {
			vector<xmlNode*> objects = GetNode(object_root[0], "object");
			for (vector<xmlNode*>::iterator it = objects.begin(); it != objects.end(); ++it) {
				string type = GetObjectType(*it);
				string id = GetObjectID(*it);
				unordered_map<string, string> elems = GetObjectAttributes(*it);

				/* Ignore 0-byte ID objects */
				if (id.size() == 0)
					continue;

				Log(DEBUG2, "Found a object: type = " + type + ", id =" + id);
				elems["id"] = id;

				shared_ptr<LTFSObject> obj;
				if (type == "LTFSNode") {
					shared_ptr<LTFSNode> node = shared_ptr<LTFSNode>(new LTFSNode(elems, session_));
					nodes_.push_back(node);
					obj = shared_ptr<LTFSObject>(node);
				} else if (type == "cartridge") {
					shared_ptr<Cartridge> cart = shared_ptr<Cartridge>(new Cartridge(elems, session_));
					cartridges_.push_back(cart);
					obj = shared_ptr<LTFSObject>(cart);
				} else if (type == "drive") {
					shared_ptr<Drive> drive = shared_ptr<Drive>(new Drive(elems, session_));
					drives_.push_back(drive);
					obj = shared_ptr<LTFSObject>(drive);
				}

				objects_.push_back(obj);
			}
		}
	} else {
		vector<string> args;
		args.push_back("Document root element is not found");
		throw XMLError(__FILE__, __LINE__, "059E", args);
	}
}
