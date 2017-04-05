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
** FILE NAME:       Drive.h
**
** DESCRIPTION:     Drive class which LTFS admin server treats
**
** AUTHORS:         Atsushi Abe
**                  IBM Tokyo Lab., Japan
**                  piste@jp.ibm.com
**
*************************************************************************************
*/
#pragma once

#include <string>

#include <stdint.h>
#include <boost/unordered_map.hpp>

#include "LTFSObject.h"

namespace ltfsadmin {

class LTFSAdminSession;

class Drive : public LTFSObject {
public:
	Drive(boost::unordered_map<std::string, std::string> elems, LTFSAdminSession *session);
	Drive(std::string, LTFSAdminSession *session);
	virtual ~Drive() {};

	// TODO (Atsushi Abe): Implement following 2 methods when Job class is introduced
	// bool get_job_schedule();
	// bool set_job_schedule();

	/** Add drive
	 *
	 *  Throw an exception inheritated from AdminLibException when drive add request is failed.
	 *
	 *  @return 0 on success, otherwise non-zero
	 */
	int Add();

	/** Remove drive
	 *
	 *  Throw an exception inheritated from AdminLibException when drive remove request is
	 *  failed.
	 *
	 *  @return 0 on success, otherwise non-zero
	 */
	int Remove();


	// TODO (Atsushi Abe): Implement following 2 methods when Job class is introduced
	// bool get_job_schedule();
	// bool set_job_schedule();

	// Accessors
	std::string   get_product_id()  { ParseElems(); return product_id_; }
	std::string   get_fw_revision() { ParseElems(); return fw_revision_;}
	std::string   get_vendor()      { ParseElems(); return vendor_;}
	std::string   get_devname()     { ParseElems(); return devname_;}
	uint16_t      get_slot()        { ParseElems(); return slot_;}
	std::string   get_node()        { ParseElems(); return node_;}
	std::string   get_status()      { ParseElems(); return status_;}

private:
	std::string   product_id_;
	std::string   fw_revision_;
	std::string   vendor_;
	std::string   devname_;
	uint16_t      slot_;
	std::string   node_;
	std::string   status_;

	virtual void ParseElems();
};

}
