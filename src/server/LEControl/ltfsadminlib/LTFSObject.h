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
** FILE NAME:       LTFSObject.h
**
** DESCRIPTION:     Base object class which LTFS admin server treats
**
** AUTHORS:         Atsushi Abe
**                  IBM Tokyo Lab., Japan
**                  piste@jp.ibm.com
**
*************************************************************************************
*/

/**
 *  @file   LTFSObject.h
 *  @brief  Base object class which LTFS admin server treats
 *  @author Atsushi Abe (piste@jp.ibm.com), IBM Tokyo Lab., Japan
 */

#pragma once

#include <string>

//#include <boost/unordered_map.hpp>

#include "LTFSAdminBase.h"

namespace ltfsadmin {

class LTFSAdminSession;

/** Emunumator of object types
 *
 *  Definition of object types
 */
typedef enum {
	LTFS_OBJ_UNKNOWN,    /**< Unknown used by default constructor */
	LTFS_OBJ_CARTRIDGE,  /**< Cartridge object type */
	LTFS_OBJ_DRIVE,      /**< Drive Object type */
	LTFS_OBJ_HISTORY,    /**< History Object Type (Not supported yet) */
	LTFS_OBJ_JOB,        /**< Job Object Type (Not supported yet) */
	LTFS_OBJ_LIBRARY,    /**< Library Object Type */
	LTFS_OBJ_LTFS_NODE,  /**< LTFS Node Object Type */
	LTFS_OBJ_PRIVILEGE,  /**< Access Privilege Type (Not supported yet) */
} ltfs_object_t;

/** Base Object class which LTFS Admin server treats
 *
 *  This is base class of all LTFS Admin Object. This object will be aqcuired by LTFSAdminSession::SessionInventory().
 */
class LTFSObject : public LTFSAdminBase {
public:
	/** The Default Constructor
	 */
	LTFSObject() : obj_type_(LTFS_OBJ_UNKNOWN) {};

	/** The Constructor
	 *
	 *  @param type Object type
	 *  @param elems Attribute list of this object
	 *  @param session Pinter to LTFSAdminSession. This is used when an action is kicked.
	 */
	LTFSObject(ltfs_object_t type, std::unordered_map<std::string, std::string> elems, LTFSAdminSession *session) :
		obj_type_(type) , elems_(elems) , elems_parsed_(false) , session_(session) {};

	/** Another Constructor to have Object by ID
	 *
	 *  @param type Object type
	 *  @param id_name Name of id of the object
	 *  @param session Pointer to LTFSAdminSession.
	 */
	LTFSObject(ltfs_object_t type, std::string id_name, LTFSAdminSession *session);

	/** The Destructor
	 */
	virtual ~LTFSObject() {};

	/** Build XML tags for request action
	 *
	 *  @return XML string for action request
	 */
	std::string ToStringForAction() {return "";};

	/** Get object ID
	 *
	 *  @return string of this object ID
	 */
	std::string& GetObjectID();

	/** Get object type
	 *
	 *  @return string of this object type
	 */
	ltfs_object_t GetObjectType();

	/** Get attribute list of this object
	 *
	 *  @return Reference of attribute list as unordered_map
	 */
	std::unordered_map<std::string, std::string>& GetAttributes();

	/** Get corresponded session pointer
	 *
	 *  @return pointer to corresponded LTFSAdminSession
	 */
	LTFSAdminSession* get_session() {return session_;}

protected:
	ltfs_object_t                                  obj_type_;     /**< Object type */
	std::string                                    obj_id_;       /**< Object ID */
	std::unordered_map<std::string, std::string> elems_;        /**< Attribute list */
	bool                                           elems_parsed_; /**< Attribute list was already interpureted and
																	   mapped to the each object by PaseElems() funtion or not. */
	virtual void ParseElems() = 0;

	LTFSAdminSession *session_; /**< Corrsponding LTFSAdminSession used by action request */
};

}
