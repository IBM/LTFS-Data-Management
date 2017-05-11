#pragma once

#include <string>

//#include <boost/unordered_map.hpp>

#include "LTFSObject.h"
#include "LTFSRequestMessage.h"

namespace ltfsadmin {

class Action:public LTFSRequestMessage {
public:
	Action() :
		LTFSRequestMessage(LTFS_MSG_ACTION, 0) , method_(""), target_(NULL) {};
	Action(uint64_t sequence, std::string method, LTFSObject* obj, std::unordered_map<std::string, std::string> options ) :
		LTFSRequestMessage(LTFS_MSG_ACTION, sequence) , method_(method), target_(obj), options_(options) {};
	virtual ~Action() {};
	virtual std::string ToString();

private:
	std::string method_;
	LTFSObject* target_;
	std::unordered_map<std::string, std::string> options_;
};

}
