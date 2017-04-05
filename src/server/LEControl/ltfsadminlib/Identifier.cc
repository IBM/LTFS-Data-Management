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
** FILE NAME:       Identifier.cc
**
** DESCRIPTION:     Identifier request message class of LTFS admin channel
**
** AUTHORS:         Atsushi Abe
**                  IBM Tokyo Lab., Japan
**                  piste@jp.ibm.com
**
*************************************************************************************
*/

#include <cstring>
#include <vector>

#include <unordered_map>

#include "Identifier.h"
#include "RequestError.h"

using namespace ltfsadmin;

Identifier::Identifier(std::string& xml) : LTFSResponseMessage(xml)
{
	auth_ = false;
};

bool Identifier::GetAuth()
{
	if (!root_)
		Parse();

	return auth_;
}

void Identifier::Parse(void)
{
	if (!root_)
		LTFSResponseMessage::Parse();

	// Parse authentication option
	if (root_) {
		std::unordered_map<std::string, std::string> opts = GetOptions(root_);

		if ( opts.count("userauth") && ! strcmp(opts["userauth"].c_str(), "required"))
			auth_ = true;
	} else {
		std::vector<std::string> args;
		args.push_back("Cannot parse authentication option in Identifier message");
		throw RequestError(__FILE__, __LINE__, "001E", args);
	}
}
