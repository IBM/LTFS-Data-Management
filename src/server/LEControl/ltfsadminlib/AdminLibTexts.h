#pragma once

#include <string>

namespace ltfsadmin {

class AdminLibTexts {
public:
	AdminLibTexts()
	{
		messages_["001I"] = "";
	}

	virtual ~AdminLibTexts() {};

protected:
	std::unordered_map<string, string> messages_;
};

}
