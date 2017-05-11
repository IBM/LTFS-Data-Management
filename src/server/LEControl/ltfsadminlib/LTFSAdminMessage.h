#pragma once

#include <string>
#include <vector>

#include <stdint.h> /* Shall be replaced to cstdint in C++11 */

#include <libxml/parser.h>
#include <libxml/tree.h>

#include "LTFSAdminBase.h"

namespace ltfsadmin {

typedef enum {
	LTFS_MSG_UNKNOWN,
	LTFS_MSG_IDENTIFIER,
	LTFS_MSG_INVENTORY,
	LTFS_MSG_ACTION,
	LTFS_MSG_RESULT,
	LTFS_MSG_LOGIN,
	LTFS_MSG_LOGOUT
} ltfs_message_t;

#define SUPPORTED_ADMIN_PROTOCOL_VERSION "2.0.0"

class LTFSAdminMessage : public LTFSAdminBase {
public:
	LTFSAdminMessage();
	LTFSAdminMessage(ltfs_message_t type, uint64_t seq);
	LTFSAdminMessage(const std::string& xml);
	virtual ~LTFSAdminMessage();
	virtual std::string ToString();
	virtual uint64_t GetSequence(){return sequence_;};
	virtual ltfs_message_t GetType(){return type_;};

protected:
	std::string    version_;
	uint64_t       sequence_;
	ltfs_message_t type_;
	std::string    xml_;

	xmlDoc         *doc_;  /* XML document */
	xmlNode        *root_; /* Root Element of XML tree  */

	virtual void Parse(void);
	virtual void Prepare(void);
};

}
