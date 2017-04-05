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
** FILE NAME:       LTFSNode.cc
**
** DESCRIPTION:     LTFSNode class which LTFS admin server treats
**
** AUTHORS:         Atsushi Abe
**                  IBM Tokyo Lab., Japan
**                  piste@jp.ibm.com
**
*************************************************************************************
*/

#include <unordered_map>
#include <memory>
#include <list>
#include <condition_variable>
#include <thread>

#include "LTFSNode.h"
#include "LTFSAdminSession.h"

using namespace ltfsadmin;

LTFSNode::LTFSNode(std::unordered_map<std::string, std::string> elems, LTFSAdminSession *session) :
	LTFSObject(LTFS_OBJ_LTFS_NODE, elems, session)
{
	admin_port_num_    = 0;
	slots_             = 0;
	used_slots_        = 0;
	remaining_slots_   = 0;
	ieslots_           = 0;
	used_ieslots_      = 0;
	remaining_ieslots_ = 0;
}

void LTFSNode::ParseElems()
{
	std::string tmp;

	// Set status "Disconnected" if session is disconnected
	if (! session_ || ! session_->is_alived() ) {
		elems_parsed_  = false;
		status_ = "Disconnected";
		return;
	}

	// Fill attribuites only first time to call this function
	if (elems_parsed_)
		return;

	obj_id_ = elems_["id"];
	if (obj_id_ == "")
		obj_id_ = "Not Available";

	node_name_ = elems_["node_name"];
	if (node_name_ == "")
		node_name_ = "Not Available";

	status_ = elems_["status"];
	if (status_ == "")
		status_ = "Not Available";

	group_id_ = elems_["group_id"];
	if (group_id_ == "")
		group_id_ = "Not Available";

	product_ = elems_["product"];
	if (product_ == "")
		product_ = "Not Available";

	version_ = elems_["version"];
	if (version_ == "")
		version_ = "Not Available";

	vendor_ = elems_["vendor"];
	if (vendor_ == "")
		vendor_ = "Not Available";

	format_spec_ = elems_["format_spec"];
	if (format_spec_ == "")
		format_spec_ = "Not Available";

	mount_point_ = elems_["mount_point"];
	if (mount_point_ == "")
		mount_point_ = "Not Available";

	work_dir_ = elems_["work_dir"];
	if (work_dir_ == "")
		work_dir_ = "Not Available";

	node_mode_ = elems_["node_mode"];
	if (node_mode_ == "")
		node_mode_ = "Not Available";

	symlink_mode_ = elems_["symlink_mode"];
	if (symlink_mode_ == "")
		symlink_mode_ = "Not Available";

	plugins_ = elems_["plugins"];
	if (plugins_ == "")
		plugins_ = "Not Available";

	cache_version_ = elems_["cache_version"];
	if (cache_version_ == "")
		cache_version_ = "Not Available";

	tmp = elems_["admin_port_num"];
	if (tmp == "")
		admin_port_num_ = UINT16_MAX;
	else
		admin_port_num_ = (uint16_t) std::stoul(tmp);

	tmp = elems_["slots"];
	if (tmp == "")
		slots_ = UINT32_MAX;
	else
		slots_ = (uint32_t) std::stoul(tmp);

	tmp = elems_["used_slots"];
	if (tmp == "")
		used_slots_ = UINT32_MAX;
	else
		used_slots_ = (uint32_t) std::stoul(tmp);

	tmp = elems_["remaining_slots"];
	if (tmp == "")
		remaining_slots_ = UINT32_MAX;
	else
		remaining_slots_ = (uint32_t) std::stoul(tmp);

	tmp = elems_["ieslots"];
	if (tmp == "")
		ieslots_ = UINT32_MAX;
	else
		ieslots_ = (uint32_t) std::stoul(tmp);

	tmp = elems_["used_ieslots"];
	if (tmp == "")
		used_ieslots_ = UINT32_MAX;
	else
		used_ieslots_ = (uint32_t) std::stoul(tmp);

	tmp = elems_["remaining_ieslots"];
	if (tmp == "")
		remaining_ieslots_ = UINT32_MAX;
	else
		remaining_ieslots_ = (uint32_t) std::stoul(tmp);

	elems_parsed_ = true;

	return;
}
