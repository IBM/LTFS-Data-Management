#pragma once

#include <string>

#include <stdint.h>

#include "LTFSObject.h"

namespace ltfsadmin {

class Library : public LTFSObject {
public:
	Library();
	virtual ~Library() {};

	// Accessors
	std::string   getProductId()  {return product_id_; }
	std::string   getFwRevision() {return fw_revision_;}
	std::string   getVendor()     {return vendor_;}
	std::string   getDevname()    {return devname_;}
	uint32_t      getDrives()     {return drives_;}
	uint32_t      getSlots()      {return slots_;}
	uint32_t      getIEslots()    {return ieslots_;}
	uint32_t      getMTEslots()   {return mteslots_;}

private:
	string   product_id_;
	string   fw_revision_;
	string   vendor_;
	string   devname_;
	uint32_t drives_;
	uint32_t slots_;
	uint32_t ieslots_;
	uint32_t mteslots_;
};

}
