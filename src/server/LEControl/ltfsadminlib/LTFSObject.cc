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
** FILE NAME:       LTFSObject.cc
**
** DESCRIPTION:     Base object class which LTFS admin server treats
**
** AUTHORS:         Atsushi Abe
**                  IBM Tokyo Lab., Japan
**                  piste@jp.ibm.com
**
*************************************************************************************
*/

#include <unordered_map>

#include "LTFSObject.h"
#include "InternalError.h"

using namespace ltfsadmin;

LTFSObject::LTFSObject(ltfs_object_t type, std::string id_name, LTFSAdminSession *session) :
		obj_type_(type), session_(session)
{
	elems_["id"]  = id_name;
	elems_parsed_ = false;
};

std::string& LTFSObject::GetObjectID()
{
	if(! obj_id_.length())
		obj_id_ = elems_["id"];

	if(! obj_id_.length()) {
		std::vector<std::string> args;
		args.push_back("Cannot get Object ID");
		throw InternalError(__FILE__, __LINE__, "031E", args);
	}

	return obj_id_;
};

ltfs_object_t LTFSObject::GetObjectType()
{
	return obj_type_;
};

std::unordered_map<std::string, std::string>& LTFSObject::GetAttributes()
{
	if(! obj_id_.length())
		obj_id_ = elems_["id"];

	if(! obj_id_.length()) {
		std::vector<std::string> args;
		args.push_back("Cannot get attributes");
		throw InternalError(__FILE__, __LINE__, "033E", args);
	}

	return elems_;
}
