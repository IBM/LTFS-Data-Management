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
** FILE NAME:       LTFSNode.h
**
** DESCRIPTION:     LTFS Node class which LTFS admin server treats
**
** AUTHORS:         Atsushi Abe
**                  IBM Tokyo Lab., Japan
**                  piste@jp.ibm.com
**
*************************************************************************************
*/
#pragma once

#define __STDC_LIMIT_MACROS

#include <string>
#include <stdint.h>

#include <boost/unordered_map.hpp>

#include "LTFSObject.h"

namespace ltfsadmin {

class LTFSAdminSession;

class LTFSNode:public LTFSObject {
public:
	LTFSNode(boost::unordered_map<std::string, std::string> elems, LTFSAdminSession *session);
	virtual ~LTFSNode() {};

	//bool force_to_sync();
	//bool migrate_cache();

	// Accessors
	std::string   get_node_name()         { ParseElems(); return node_name_;}
	std::string   get_status()            { ParseElems(); return status_;}
	std::string   get_group_id()          { ParseElems(); return group_id_;}
	std::string   get_product()           { ParseElems(); return product_;}
	std::string   get_version()           { ParseElems(); return version_;}
	std::string   get_vendor()            { ParseElems(); return vendor_;}
	std::string   get_format_spec()       { ParseElems(); return format_spec_;}
	std::string   get_mount_point()       { ParseElems(); return mount_point_;}
	std::string   get_work_dir()          { ParseElems(); return work_dir_;}
	std::string   get_node_mode()         { ParseElems(); return node_mode_;}
	std::string   get_symlink_mode()      { ParseElems(); return symlink_mode_;}
	std::string   get_plugins()           { ParseElems(); return plugins_;}
	std::string   get_cache_version()     { ParseElems(); return cache_version_;}
	uint16_t      get_admin_port_num()    { ParseElems(); return admin_port_num_;}
	uint32_t      get_slots()             { ParseElems(); return slots_;}
	uint32_t      get_used_slots()        { ParseElems(); return used_slots_;}
	uint32_t      get_remaining_slots()   { ParseElems(); return remaining_slots_;}
	uint32_t      get_ieslots()           { ParseElems(); return ieslots_;}
	uint32_t      get_used_ieslots()      { ParseElems(); return used_ieslots_;}
	uint32_t      get_remaining_ieslots() { ParseElems(); return remaining_ieslots_;}

private:
	std::string   node_name_;
	std::string   status_;
	std::string   group_id_;
	std::string   product_;
	std::string   version_;
	std::string   vendor_;
	std::string   format_spec_;
	std::string   mount_point_;
	std::string   work_dir_;
	std::string   node_mode_;
	std::string   symlink_mode_;
	std::string   plugins_;
	std::string   cache_version_;
	uint16_t      admin_port_num_;
	uint32_t      slots_;
	uint32_t      used_slots_;
	uint32_t      remaining_slots_;
	uint32_t      ieslots_;
	uint32_t      used_ieslots_;
	uint32_t      remaining_ieslots_;

	virtual void ParseElems();
};

}
