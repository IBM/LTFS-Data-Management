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
** FILE NAME:       Result.h
**
** DESCRIPTION:     Result message class of LTFS admin channel
**
** AUTHORS:         Atsushi Abe
**                  IBM Tokyo Lab., Japan
**                  piste@jp.ibm.com
**
*************************************************************************************
*/
#pragma once

#include <string>
#include <list>

#include <boost/unordered_map.hpp>
#include <boost/shared_ptr.hpp>

#include "LTFSResponseMessage.h"

namespace ltfsadmin {

class LTFSObject;
class LTFSNode;
class Drive;
class Cartridge;
class LTFSAdminSession;

typedef enum {
	LTFS_ADMIN_SUCCESS = 0,
	LTFS_ADMIN_ERROR,
	LTFS_ADMIN_DISCONNECTED,
	LTFS_ADMIN_UNKNOWN
} ltfs_status_t;

class Result : public LTFSResponseMessage {
public:
	Result(std::string& xml, LTFSAdminSession *session);
	virtual ~Result() {};
	ltfs_status_t GetStatus(void);
	std::string GetOutput(void);
	std::list<boost::shared_ptr <LTFSObject> >& GetObjects();
	std::list<boost::shared_ptr <LTFSNode> >& GetLTFSNodes();
	std::list<boost::shared_ptr <Drive> >& GetDrives();
	std::list<boost::shared_ptr <Cartridge> >& GetCartridges();

private:
	LTFSAdminSession *session_;

	ltfs_status_t status_;
	std::string output_;

	std::list<boost::shared_ptr <LTFSObject> > objects_;
	std::list<boost::shared_ptr <LTFSNode> >   nodes_;
	std::list<boost::shared_ptr <Drive> >      drives_;
	std::list<boost::shared_ptr <Cartridge> >  cartridges_;

	std::string GetObjectType(xmlNode* obj);
	std::string GetObjectID(xmlNode* obj);
	boost::unordered_map<string, string> GetObjectAttributes(xmlNode* obj);

	virtual void Parse(void);
};

}
