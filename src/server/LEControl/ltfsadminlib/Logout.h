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
** FILE NAME:       Logout.h
**
** DESCRIPTION:     Logout request message class of LTFS admin channel
**
** AUTHORS:         Atsushi Abe
**                  IBM Tokyo Lab., Japan
**                  piste@jp.ibm.com
**
*************************************************************************************
*/

#include <string>

using namespace std;

namespace ltfsadmin {

class Logout:public LTFSRequestMessage {
public:
	Logout() :
		LTFSRequestMessage(LTFS_MSG_LOGOUT, 0) {};
	Logout(uint64_t sequence) :
		LTFSRequestMessage(LTFS_MSG_LOGOUT, sequence) {};
	virtual ~Logout() {};
};

}
