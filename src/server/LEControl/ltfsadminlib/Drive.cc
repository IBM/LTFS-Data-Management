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
** FILE NAME:       Drive.cc
**
** DESCRIPTION:     Drive class which LTFS admin server treats
**
** AUTHORS:         Atsushi Abe
**                  IBM Tokyo Lab., Japan
**                  piste@jp.ibm.com
**
*************************************************************************************
*/

#include "Drive.h"

#include <boost/lexical_cast.hpp>

#include "LTFSAdminSession.h"
#include "Action.h"
#include "Result.h"
#include "RequestError.h"

using namespace std;
using namespace boost;
using namespace ltfsadmin;

Drive::Drive(unordered_map<string, string> elems, LTFSAdminSession *session) :
	LTFSObject(LTFS_OBJ_DRIVE, elems, session)
{
	slot_ = 0;
}

Drive::Drive(string drive_name, LTFSAdminSession *session) :
	LTFSObject(LTFS_OBJ_DRIVE, drive_name, session)
{
	slot_ = 0;
}

int Drive::Add()
{
	int rc = 0;
	unordered_map<string, string> empty;

	shared_ptr<Action> act = shared_ptr<Action>(new Action(session_->GetSequence(), "add", this, empty));
	shared_ptr<Result> res = session_->SessionAction(act);

	ltfs_status_t status = res->GetStatus();
	if (status == LTFS_ADMIN_SUCCESS)
		rc = 0;
	else {
		vector<string> args;
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
	unordered_map<string, string> empty;

	shared_ptr<Action> act = shared_ptr<Action>(new Action(session_->GetSequence(), "remove", this, empty));
	shared_ptr<Result> res = session_->SessionAction(act);

	ltfs_status_t status = res->GetStatus();
	if (status == LTFS_ADMIN_SUCCESS)
		rc = 0;
	else {
		vector<string> args;
		args.push_back("Drive remove is failed");
		args.push_back(res->GetOutput());
		throw RequestError(__FILE__, __LINE__, "081E", args);
		rc = -1;
	}

	return rc;
}

void Drive::ParseElems()
{
	string tmp;

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
		slot_ = lexical_cast<uint16_t>(tmp);

	node_ = elems_["node"];
	if (node_ == "")
		node_ = "Not Available";

	status_ = elems_["status"];
	if (status_ == "")
		status_ = "Not Available";

	elems_parsed_ = true;
}
