#pragma once

#include <string>

#include "LTFSResponseMessage.h"

namespace ltfsadmin {

class Identifier:public LTFSResponseMessage {
public:
	Identifier(std::string& xml);
	virtual ~Identifier(){};
	bool GetAuth();

private:
	bool auth_;
	virtual void Parse(void);
};

}
