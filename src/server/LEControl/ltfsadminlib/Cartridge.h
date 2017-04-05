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
** FILE NAME:       Cartridge.h
**
** DESCRIPTION:     Cartridge class which LTFS admin server treats
**
** AUTHORS:         Atsushi Abe
**                  IBM Tokyo Lab., Japan
**                  piste@jp.ibm.com
**
*************************************************************************************
*/
#pragma once

#include <string>

#include <stdint.h>
#include <boost/unordered_map.hpp>

#include "LTFSObject.h"

namespace ltfsadmin {

class LTFSADminSession;

/** Emunumator of object types
 *
 *  Definition of object types
 */
typedef enum {
	SLOT_HOME,  /**< Home slot */
	SLOT_IE,    /**< IE slot */
	SLOT_DRIVE, /**< Drive slot */
} ltfs_slot_t;

class Cartridge : public LTFSObject {
public:
	Cartridge(boost::unordered_map<std::string, std::string> elems, LTFSAdminSession *session);
	Cartridge(string cart_name, LTFSAdminSession *session);
	virtual ~Cartridge() {};

	/*
	bool format();
	bool unformat();
	bool recovery();
	bool list_rollback_points();
	bool rollback(int64_t generation);
	*/

	/** Add cartridge
	 *
	 *  Throw an exception inheritated from AdminLibException when cartridge add request is failed
	 *
	 *  @return 0 on success, otherwise non-zero;
	 */
	int Add();

	/** Remove cartridge
	 *
	 *  Throw an exception inheritated from AdminLibException when cartridge remove request is failed
	 *
	 *  @param keep_cache Do not remove teh current cache files
	 *  @param keep_on_drive Do not eject from the drive
	 *  @param force force remove when this value is true
	 *  @return 0 on success, otherwise non-zero;
	 */
	int Remove(bool keep_cache = true, bool keep_on_drive = false, bool force = false);

	/** Mount cartridge
	 *
	 *  Throw an exception inheritated from AdminLibException when cartridge remove request is failed or
	 *  target drive to mount is ""
	 *
	 *  @param drive_serial drive serial to mount
	 *  @return 0 on success, otherwise non-zero;
	 */
	int Mount(string drive_serial);

	/** Unmount cartridge
	 *
	 *  Throw an exception inheritated from AdminLibException when cartridge remove request is failed
	 *
	 *  @return 0 on success, otherwise non-zero;
	 */
	int Unmount();

	/** Sync cartridge
	 *  Throw an exception inheritated from AdminLibException when cartridge remove request is failed
	 *
	 *  @return 0 on success, otherwise non-zero;
	 */
	int Sync();

	/** Format cartridge
	 *  Throw an exception inheritated from AdminLibException when cartridge remove request is failed
	 *
	 *  @drive_serial drive serial to format
	 *  @density_code density code to format
	 *  @force force flag
	 *  @return 0 on success, otherwise non-zero;
	 */
	int Format(string drive_serial, uint8_t density_code = 0, bool force = false);

	/** Check cartridge (Automatic recovery, ltfsck)
	 *  Throw an exception inheritated from AdminLibException when cartridge remove request is failed
	 *
	 *  @drive_serial drive serial to format
	 *  @deep deep_recovery flag
	 *  @return 0 on success, otherwise non-zero;
	 */
	int Check(string drive_serial, bool deep = false);

	/** Move cartridge
	 *
	 *  Throw an exception inheritated from AdminLibException when cartridge move request is failed
	 *
	 *  @param dest destination slot type
	 *  @param drive_serial drive serial only available when dest is SLOT_DRIVE
	 *  @return 0 on success, otherwise non-zero;
	 */
	int Move(ltfs_slot_t dest, string drive_serial = "");

	// Accessors
	uint16_t    get_home_slot()          { ParseElems(); return home_slot_;}
	uint16_t    get_slot()               { ParseElems(); return slot_;}
	string      get_slot_type()          { ParseElems(); return slot_type_;}
	uint64_t    get_total_cap()          { ParseElems(); return total_cap_;}
	uint64_t    get_remaining_cap()      { ParseElems(); return remaining_cap_;}
	uint64_t    get_total_blocks()       { ParseElems(); return total_blocks_;}
	uint64_t    get_valid_blocks()       { ParseElems(); return valid_blocks_;}
	uint8_t     get_density_code()       { ParseElems(); return density_code_;}
	string      get_mount_node()         { ParseElems(); return mount_node_;}
	string      get_status()             { ParseElems(); return status_;}
	bool        get_placed_by_operator() { ParseElems(); return placed_by_operator_;}

private:
	uint16_t home_slot_;
	uint16_t slot_;
	string   slot_type_;
	uint64_t total_cap_;
	uint64_t remaining_cap_;
	uint64_t total_blocks_;
	uint64_t valid_blocks_;
	uint8_t  density_code_;
	string   mount_node_;
	string   status_;
	bool     placed_by_operator_;

	/** Move cartridge (Internal function)
	 *
	 *  Move cartridge without input error cheking. The destination shall be specified in options.
	 *  drive serial can be specified only when destination is "drive"
	 *
	 *  @param options "destination" and "drive_serial" is available as key.
	 *                 On "destination" key, "homeslot", "ieslot" or "drive" is available as value
	 *                 "drive_serial" key is only available when option["destination"] is "drive".
	 *                 drive sserial number chall be specified as value.
	 *  @return 0 on success, otherwise non-zero;
	 */
	int Move(boost::unordered_map<std::string, std::string> options);

	virtual void ParseElems();
};

}
