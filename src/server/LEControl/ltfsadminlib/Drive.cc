#include <memory>
#include <list>
#include <condition_variable>
#include <thread>

#include "LTFSAdminSession.h"
#include "Action.h"
#include "Result.h"
#include "RequestError.h"

#include "Drive.h"

using namespace ltfsadmin;

Drive::Drive(std::unordered_map<std::string, std::string> elems, LTFSAdminSession *session) :
	LTFSObject(LTFS_OBJ_DRIVE, elems, session)
{
	slot_ = 0;
}

Drive::Drive(std::string drive_name, LTFSAdminSession *session) :
	LTFSObject(LTFS_OBJ_DRIVE, drive_name, session)
{
	slot_ = 0;
}

int Drive::Add()
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
		args.push_back("Drive add is failed");
		args.push_back(res->GetOutput());
		throw RequestError(__FILE__, __LINE__, "080E", args);
		rc = -1;
	}

	return rc;
}

int Drive::Remove()
{
	int rc = 0;
	std::unordered_map<std::string, std::string> empty;

	std::shared_ptr<Action> act = std::shared_ptr<Action>(new Action(session_->GetSequence(), "remove", this, empty));
	std::shared_ptr<Result> res = session_->SessionAction(act);

	ltfs_status_t status = res->GetStatus();
	if (status == LTFS_ADMIN_SUCCESS)
		rc = 0;
	else {
		std::vector<std::string> args;
		args.push_back("Drive remove is failed");
		args.push_back(res->GetOutput());
		throw RequestError(__FILE__, __LINE__, "081E", args);
		rc = -1;
	}

	return rc;
}

void Drive::ParseElems()
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

	product_id_ = elems_["product_id"];
	if (product_id_ == "")
		product_id_ = "Not Available";

	fw_revision_ = elems_["fw_revision"];
	if (fw_revision_ == "")
		fw_revision_ = "Not Available";

	vendor_ = elems_["vendor"];
	if (vendor_ == "")
		vendor_ = "Not Available";

	devname_ = elems_["devname"];
	if (devname_ == "")
		devname_ = "Not Available";

	tmp = elems_["slot"];
	if (tmp == "")
		slot_ = 0;
	else
		slot_ = (uint16_t) std::stoul(tmp);

	node_ = elems_["node"];
	if (node_ == "")
		node_ = "Not Available";

	status_ = elems_["status"];
	if (status_ == "")
		status_ = "Not Available";

	elems_parsed_ = true;
}
