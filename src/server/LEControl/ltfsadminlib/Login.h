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
** FILE NAME:       Login.h
**
** DESCRIPTION:     Login request message class of LTFS admin channel
**
** AUTHORS:         Atsushi Abe
**                  IBM Tokyo Lab., Japan
**                  piste@jp.ibm.com
**
*************************************************************************************
*/

#include <string>

#include "LTFSRequestMessage.h"

namespace ltfsadmin {

class Login : public LTFSRequestMessage {
public:
	Login() :
		LTFSRequestMessage(LTFS_MSG_LOGIN, 0) , user_(""), password_("") {};
	Login(uint64_t sequence, std::string user = "", std::string password = "") :
		LTFSRequestMessage(LTFS_MSG_LOGIN, sequence) , user_(user) , password_(password) {};
	virtual ~Login() {};
	virtual std::string ToString();

private:
	std::string user_;
	std::string password_;
};

}
