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
	static std::shared_ptr<LTFSAdminSession> Connect(std::string ip_address, uint16_t port);
	static std::shared_ptr<LTFSAdminSession> Reconnect(std::shared_ptr<LTFSAdminSession> session);
	static void Disconnect(std::shared_ptr<LTFSAdminSession> session);
	static std::shared_ptr<LTFSNode> InventoryNode(std::shared_ptr<LTFSAdminSession> session);
	static std::shared_ptr<Drive> InventoryDrive(std::string id,
												   std::shared_ptr<LTFSAdminSession> session,
												   bool force = false);
	static int InventoryDrive(std::list<std::shared_ptr<Drive> > &drives,
							  std::shared_ptr<LTFSAdminSession> session,
							  bool assigned_only = true,
							  bool force = false);
	static std::shared_ptr<Cartridge> InventoryCartridge(std::string id,
														   std::shared_ptr<LTFSAdminSession> session,
														   bool force = false);
	static int InventoryCartridge(std::list<std::shared_ptr<Cartridge> > &cartridges,
								  std::shared_ptr<LTFSAdminSession> session,
								  bool assigned_only = true,
								  bool force = false);
	static int AssignDrive(std::string serial, std::shared_ptr<LTFSAdminSession> session);
	static int UnassignDrive(std::shared_ptr<Drive> drive);
	static int AssignCartridge(std::string barcode, std::shared_ptr<LTFSAdminSession> session, std::string drive_serial = "");
	static int UnassignCartridge(std::shared_ptr<Cartridge> cartridge, bool keep_on_drive = false);

	static int MountCartridge(std::shared_ptr<Cartridge> cartridge, std::string drive_serial);
	static int UnmountCartridge(std::shared_ptr<Cartridge> cartridge);
	static int SyncCartridge(std::shared_ptr<Cartridge> cartridge);
	static int FormatCartridge(std::shared_ptr<Cartridge> cartridge, std::string drive_serial, uint8_t density_code = 0, bool force = false);
	static int CheckCartridge(std::shared_ptr<Cartridge> cartridge, std::string drive_serial, bool deep = false);

	static int MoveCartridge(std::shared_ptr<Cartridge> cartridge,
						  ltfs_slot_t slot_type, std::string drive_serial = "");

	// Move Cartridge Assign
	// Get Offset file
	// Get index file

private:
	static const std::unordered_map<std::string, int> format_errors_;
	static LTFSAdminLog *logger_;
};
