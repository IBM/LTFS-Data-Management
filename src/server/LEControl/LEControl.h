/*
**  %Z% %I% %W% %G% %U%
**
**  ZZ_Copyright_BEGIN
**
**
**  IBM Confidential
**
**  OCO Source Materials
**
**  IBM Linear Tape File System Enterprise Edition
**
**  (C) Copyright IBM Corp. 2012
**
**  The source code for this program is not published or other-
**  wise divested of its trade secrets, irrespective of what has
**  been deposited with the U.S. Copyright Office
**
**
**  ZZ_Copyright_END
*/

#pragma once

#include <syslog.h>

#include "./ltfsadminlib/LTFSAdminLog.h"
#include "./ltfsadminlib/LTFSAdminSession.h"
#include "./ltfsadminlib/InternalError.h"
#include "./ltfsadminlib/Drive.h"
#include "./ltfsadminlib/Cartridge.h"
#include "./ltfsadminlib/LTFSNode.h"

using namespace ltfsadmin;

enum ltfsle_drive_status {
	DRIVE_LTFS_STATUS_AVAILABLE = 0, // Available for LTFS to use
	DRIVE_LTFS_STATUS_UNAVAILABLE,   // Not available for LTFS to use
	DRIVE_LTFS_STATUS_ERROR,         // Problem identified with drive
	DRIVE_LTFS_STATUS_NOT_INSTALLED, // The drive is not installed
	DRIVE_LTFS_STATUS_LOCKED,        // The drive is locked to keep "Critical" cartridge
	DRIVE_LTFS_STATUS_DISCONNECTED,  // Session to the LTFS nod is down
};

enum ltfsle_tape_status {
	TAPE_STATUS_UNFORMATTED = 0,
	TAPE_STATUS_VALID_LTFS,
	TAPE_STATUS_INVALID_LTFS,       // Not a valid LTFS volume, need to run LTFSCK
	TAPE_STATUS_UNKNOWN,            // No barcode or barcode conflicts with existing barcode
	TAPE_STATUS_UNAVAILABLE,        // Locked in drive or otherwise not accessible
	TAPE_STATUS_WARNING,            // A error was reported for this tape
	TAPE_STATUS_ERROR,              // Problem identified with tape
	TAPE_STATUS_CRITICAL,           // Index is dirty but the volume is read-only
	TAPE_STATUS_INACCESIBLE,        // Can't access to the tape
	TAPE_STATUS_CLEANING,           // Cleaning tape
	TAPE_STATUS_WRITE_PROTECTED=10, // Write Protected tape
	TAPE_STATUS_DUPLICATED,         // More than two cartridga have the same barcode
	TAPE_STATUS_NON_SUPPORTED,      // Non supported medium
	TAPE_STATUS_IN_PROGRESS,        // In progress
	TAPE_STATUS_DISCONNECTED,       // Session to the LTFS nod is down
	TAPE_STATUS_UNSPECIFIED         // Unspecified (Actually only used into EE)
};

enum ltfsle_node_status {
	NODE_STATUS_AVAILABLE = 0,
	NODE_STATUS_OUT_OF_SYNC,
	NODE_STATUS_LICENSE_EXPIRED,
	NODE_STATUS_DISCONNECTED,    // Session is down
	NODE_STATUS_NOT_CONFIGURED,  // LE is not configured correctly   
	NODE_STATUS_UNKNOWN          // should be max value in node status
};

enum ltfsle_tape_location {
	LOCATION_MEDIUM_TRANSPORT_ELEMENT = 0,
	LOCATION_MEDIUM_STORAGE_ELEMENT,
	LOCATION_IMPORT_EXPORT_SLOT,
	LOCATION_DATA_TRANSFER_ELEMENT
};

class LEControl {
public:
	static boost::shared_ptr<LTFSAdminSession> Connect(std::string ip_address, uint16_t port);
	static boost::shared_ptr<LTFSAdminSession> Reconnect(boost::shared_ptr<LTFSAdminSession> session);
	static void Disconnect(boost::shared_ptr<LTFSAdminSession> session);
	static boost::shared_ptr<LTFSNode> InventoryNode(boost::shared_ptr<LTFSAdminSession> session);
	static boost::shared_ptr<Drive> InventoryDrive(std::string id,
												   boost::shared_ptr<LTFSAdminSession> session,
												   bool force = false);
	static int InventoryDrive(std::list<boost::shared_ptr<Drive> > &drives,
							  boost::shared_ptr<LTFSAdminSession> session,
							  bool assigned_only = true,
							  bool force = false);
	static boost::shared_ptr<Cartridge> InventoryCartridge(std::string id,
														   boost::shared_ptr<LTFSAdminSession> session,
														   bool force = false);
	static int InventoryCartridge(std::list<boost::shared_ptr<Cartridge> > &cartridges,
								  boost::shared_ptr<LTFSAdminSession> session,
								  bool assigned_only = true,
								  bool force = false);
	static int AssignDrive(std::string serial, boost::shared_ptr<LTFSAdminSession> session);
	static int UnassignDrive(boost::shared_ptr<Drive> drive);
	static int AssignCartridge(std::string barcode, boost::shared_ptr<LTFSAdminSession> session, std::string drive_serial = "");
	static int UnassignCartridge(boost::shared_ptr<Cartridge> cartridge, bool keep_on_drive = false);

	static int MountCartridge(boost::shared_ptr<Cartridge> cartridge, std::string drive_serial);
	static int UnmountCartridge(boost::shared_ptr<Cartridge> cartridge);
	static int SyncCartridge(boost::shared_ptr<Cartridge> cartridge);
	static int FormatCartridge(boost::shared_ptr<Cartridge> cartridge, std::string drive_serial, uint8_t density_code = 0, bool force = false);
	static int CheckCartridge(boost::shared_ptr<Cartridge> cartridge, std::string drive_serial, bool deep = false);

	static int MoveCartridge(boost::shared_ptr<Cartridge> cartridge,
						  ltfs_slot_t slot_type, std::string drive_serial = "");

	// Move Cartridge Assign
	// Get Offset file
	// Get index file

private:
	static const boost::unordered_map<std::string, int> format_errors_;
	static LTFSAdminLog *logger_;
};

#include "mmm/debug.h"

class LELogger : public LTFSAdminLog {
public:
	virtual void Log(loglevel_t level, string s)
	{
		if (level <= INFO) {
			syslog(LOG_WARNING, s.c_str());
			dbgprintf(0, "%s", s.c_str());
		} else if (level <= GetLogLevel()) {
			int level_mmm = 3;
			switch((int)level) {
				case DEBUG0:
				case DEBUG1:
					level_mmm = 1;
					break;
				case DEBUG2:
					level_mmm = 2;
					break;
				case DEBUG3:
					level_mmm = 3;
					break;
				default:
					break;
			}
			dbgprintf(level_mmm, "%s", s.c_str());
		}
	}

	virtual void Msg(loglevel_t level, string id, ...)
	{
		va_list ap;
		va_start(ap, id);
		Msg(level, id, ap);
		va_end(ap);
	}

	virtual void Msg(loglevel_t level, string id, va_list ap)
	{
		if (LTFSAdminLog::msg_table_.count(id)) {
			if (level <= GetLogLevel()) {
				char arg[MAX_LEN_MSG_BUF + 1];
				string fmt = LTFSAdminLog::msg_table_[id];
				fmt = "GLESA" + id + ": " + fmt;
				strncpy(arg, fmt.c_str(), MAX_LEN_MSG_BUF);
				vsyslog(LOG_INFO, arg, ap);
			}
		}
	}
};
