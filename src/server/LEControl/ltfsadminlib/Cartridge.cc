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
** FILE NAME:       Cartridge.cc
**
** DESCRIPTION:     Cartridge class which LTFS admin server treats
**
** AUTHORS:         Atsushi Abe
**                  IBM Tokyo Lab., Japan
**                  piste@jp.ibm.com
**
*************************************************************************************
*/

#include <sstream>
#include <iomanip>
#include <unordered_map>
#include <memory>
#include <list>
#include <condition_variable>
#include <thread>


#include "LTFSAdminSession.h"
#include "Action.h"
#include "Result.h"
#include "RequestError.h"

#include "Cartridge.h"

using namespace ltfsadmin;

Cartridge::Cartridge(std::unordered_map<std::string, std::string> elems, LTFSAdminSession *session) :
	LTFSObject(LTFS_OBJ_CARTRIDGE, elems, session)
{
}

Cartridge::Cartridge(std::string cart_name, LTFSAdminSession *session) :
	LTFSObject(LTFS_OBJ_CARTRIDGE, cart_name, session)
{
}

int Cartridge::Add()
{
	int rc = 0;
	std::unordered_map<std::string, std::string> empty;

	std::shared_ptr<Action> act = std::shared_ptr<Action>(new Action(session_->GetSequence(), "add", this, empty));
	std::shared_ptr<Result> res = session_->SessionAction(act);

	ltfs_status_t status = res->GetStatus();
	if (status == LTFS_ADMIN_SUCCESS)
		rc = 0;
	else {
		std::vector<std::string> args;
		args.push_back("Cartridge add is failed");
		args.push_back(res->GetOutput());
		throw RequestError(__FILE__, __LINE__, "070E", args);
		rc = -1;
	}

	return rc;
}

int Cartridge::Remove(bool keep_cache, bool keep_on_drive, bool force)
{
	int rc = 0;
	std::unordered_map<std::string, std::string> options;

	if (force)
		options["force"] = "true";

	if (keep_cache)
		options["keep_cache"] = "VALID";

	if (keep_on_drive)
		options["keep_on_drive"] = "true";

	std::shared_ptr<Action> act = std::shared_ptr<Action>(new Action(session_->GetSequence(), "remove", this, options));
	std::shared_ptr<Result> res = session_->SessionAction(act);

	ltfs_status_t status = res->GetStatus();
	if (status == LTFS_ADMIN_SUCCESS || force)
		rc = 0;
	else {
		std::vector<std::string> args;
		args.push_back("Cartridge remove is failed");
		args.push_back(res->GetOutput());
		throw RequestError(__FILE__, __LINE__, "071E", args);
		rc = -1;
	}

	return rc;
}

int Cartridge::Mount(std::string drive_serial)
{
	int rc = 0;
	std::unordered_map<std::string, std::string> options;

	if (!drive_serial.length()) {
		std::vector<std::string> args;
		args.push_back("No target drive is specified");
		throw RequestError(__FILE__, __LINE__, "075E", args);
		return -1;
	}

	options["destination"] = "drive";
	options["drive_serial"] = drive_serial;

	std::shared_ptr<Action> act = std::shared_ptr<Action>(new Action(session_->GetSequence(), "mount", this, options));
	std::shared_ptr<Result> res = session_->SessionAction(act);

	ltfs_status_t status = res->GetStatus();
	if (status == LTFS_ADMIN_SUCCESS)
		rc = 0;
	else {
		std::vector<std::string> args;
		args.push_back("Cartridge mount is failed");
		args.push_back(res->GetOutput());
		throw RequestError(__FILE__, __LINE__, "076E", args);
		rc = -1;
	}

	return rc;
}

int Cartridge::Unmount()
{
	int rc = 0;
	std::unordered_map<std::string, std::string> empty;

	std::shared_ptr<Action> act = std::shared_ptr<Action>(new Action(session_->GetSequence(), "unmount", this, empty));
	std::shared_ptr<Result> res = session_->SessionAction(act);

	ltfs_status_t status = res->GetStatus();
	if (status == LTFS_ADMIN_SUCCESS)
		rc = 0;
	else {
		std::vector<std::string> args;
		args.push_back("Cartridge unmount is failed");
		args.push_back(res->GetOutput());
		throw RequestError(__FILE__, __LINE__, "077E", args);
		rc = -1;
	}

	return rc;
}

int Cartridge::Sync()
{
	int rc = 0;
	std::unordered_map<std::string, std::string> empty;

	std::shared_ptr<Action> act = std::shared_ptr<Action>(new Action(session_->GetSequence(), "sync", this, empty));
	std::shared_ptr<Result> res = session_->SessionAction(act);

	ltfs_status_t status = res->GetStatus();
	if (status == LTFS_ADMIN_SUCCESS)
		rc = 0;
	else {
		std::vector<std::string> args;
		args.push_back("Cartridge sync is failed");
		args.push_back(res->GetOutput());
		throw RequestError(__FILE__, __LINE__, "078E", args);
		rc = -1;
	}

	return rc;
}

int Cartridge::Format(std::string drive_serial, uint8_t density_code, bool force)
{
	int rc = 0;
	std::unordered_map<std::string, std::string> options;

	if (!drive_serial.length()) {
		std::vector<std::string> args;
		args.push_back("No target drive is specified");
		throw RequestError(__FILE__, __LINE__, "079E", args);
		return -1;
	}

	options["drive_serial"] = drive_serial;

	if (density_code) {
		std::ostringstream os;
		os << "0x" << std::setw(2) << std::setfill('0') << std::hex << (unsigned int)density_code;
		options["density"] = os.str();
	}

	if (force)
		options["force"] = "true";

	std::shared_ptr<Action> act = std::shared_ptr<Action>(new Action(session_->GetSequence(), "format", this, options));
	std::shared_ptr<Result> res = session_->SessionAction(act);

	ltfs_status_t status = res->GetStatus();
	if (status == LTFS_ADMIN_SUCCESS)
		rc = 0;
	else {
		std::vector<std::string> args;
		args.push_back("Cartridge format is failed");
		args.push_back(res->GetOutput());
		throw RequestError(__FILE__, __LINE__, "080E", args);
		rc = -1;
	}

	return rc;
}

int Cartridge::Check(std::string drive_serial, bool deep)
{
	int rc = 0;
	std::unordered_map<std::string, std::string> options;

	if (!drive_serial.length()) {
		std::vector<std::string> args;
		args.push_back("No target drive is specified");
		throw RequestError(__FILE__, __LINE__, "081E", args);
		return -1;
	}

	options["drive_serial"] = drive_serial;

	if (deep)
		options["deep_recovery"] = "true";

	std::shared_ptr<Action> act = std::shared_ptr<Action>(new Action(session_->GetSequence(), "recovery", this, options));
	std::shared_ptr<Result> res = session_->SessionAction(act);

	ltfs_status_t status = res->GetStatus();
	if (status == LTFS_ADMIN_SUCCESS)
		rc = 0;
	else {
		std::vector<std::string> args;
		args.push_back("Cartridge recovery is failed");
		args.push_back(res->GetOutput());
		throw RequestError(__FILE__, __LINE__, "082E", args);
		rc = -1;
	}

	return rc;
}

int Cartridge::Move(ltfs_slot_t dest, std::string drive_serial)
{
	std::unordered_map<std::string, std::string> options;

	switch (dest) {
		case SLOT_HOME:
			options["destination"] = "homeslot";
			break;
		case SLOT_IE:
			options["destination"] = "ieslot";
			break;
		case SLOT_DRIVE:
			options["destination"] = "drive";
			if (drive_serial.length())
				options["drive_serial"] = drive_serial;
			else {
				std::vector<std::string> args;
				args.push_back("Drive serial is required");
				throw RequestError(__FILE__, __LINE__, "072E", args);
				return -1;
			}
			break;
		default:
			std::vector<std::string> args;
			args.push_back("Invalid destination");
			throw RequestError(__FILE__, __LINE__, "073E", args);
			return -1;
			break;
	}

	return Move(options);
}

int Cartridge::Move(std::unordered_map<std::string, std::string> options)
{
	int rc = 0;

	std::shared_ptr<Action> act = std::shared_ptr<Action>(new Action(session_->GetSequence(), "move", this, options));
	std::shared_ptr<Result> res = session_->SessionAction(act);

	ltfs_status_t status = res->GetStatus();
	if (status == LTFS_ADMIN_SUCCESS)
		rc = 0;
	else {
		std::vector<std::string> args;
		args.push_back("Cartridge move is failed");
		args.push_back(res->GetOutput());
		throw RequestError(__FILE__, __LINE__, "074E", args);
		rc = -1;
	}

	return rc;
}

void Cartridge::ParseElems()
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

	tmp = elems_["home_slot"];
	if (tmp == "")
		home_slot_ = 0;
	else
		home_slot_ = (uint16_t) std::stoul(tmp);

	tmp = elems_["slot"];
	if (tmp == "")
		slot_ = 0;
	else
		slot_ = (uint16_t) std::stoul(tmp);

	slot_type_ = elems_["slot_type"];
	if (slot_type_ == "")
		slot_type_ = "Not Available";

	tmp = elems_["total_cap"];
	if (tmp == "")
		total_cap_ = 0;
	else
		total_cap_ = (uint64_t) std::stoul(tmp);

	tmp = elems_["remaining_cap"];
	if (tmp == "")
		remaining_cap_ = 0;
	else
		remaining_cap_ = (uint64_t) std::stoul(tmp);

	tmp = elems_["total_blocks"];
	if (tmp == "")
		total_blocks_ = 0;
	else
		total_blocks_ = (uint64_t) std::stoul(tmp);

	tmp = elems_["valid_blocks"];
	if (tmp == "")
		valid_blocks_ = 0;
	else
		valid_blocks_ = (uint64_t) std::stoul(tmp);

	tmp = elems_["density_code"];
	if (tmp == "")
		density_code_ = 0;
	else {
		std::stringstream ss;
		int temp_value;

		ss << tmp;
		ss >> std::hex >> temp_value;
		density_code_ = (uint8_t)temp_value;
	}

	mount_node_ = elems_["mount_node"];

	status_ = elems_["status"];
	if (status_ == "")
		status_ = "Not Available";

	tmp = elems_["placed_by_operator"];
	if (tmp == "true")
		placed_by_operator_ = true;
	else
		placed_by_operator_ = false;

	elems_parsed_ = true;
}
