#pragma once

#include <string>

#include "LTFSAdminMessage.h"

namespace ltfsadmin {

class LTFSRequestMessage:public LTFSAdminMessage {
public:
	LTFSRequestMessage() {};
	LTFSRequestMessage(ltfs_message_t type, uint64_t seq) : LTFSAdminMessage(type, seq) { };
	virtual ~LTFSRequestMessage() {};

protected:
	virtual void Parse(void) {}; /* Parse function is not needed to request message classes */
};

}
