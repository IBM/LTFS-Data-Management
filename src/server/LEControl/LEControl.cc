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
**  (C) Copyright IBM Corp. 2015
**
**  The source code for this program is not published or other-
**  wise divested of its trade secrets, irrespective of what has
**  been deposited with the U.S. Copyright Office
**
**
**  ZZ_Copyright_END
*/

#include  <memory>

#include "LEControl.h"
#include "StatusConv.h"

LTFSAdminLog *LEControl::logger_ = NULL;

/*
 *  Following enumdefinitil is quite bad...
 *  We need to define those in one place but same definitions are in
 *   - ltfsee.cc
 *   - mmm.h
 */
enum {
	FORMAT_JAG4 = 0x54,
	FORMAT_JAG4W = 0x74,
	FORMAT_JAG5 = 0x55,
	FORMAT_JAG5W = 0x75,
};

std::shared_ptr<LTFSAdminSession> LEControl::Connect(string node_addr, uint16_t port_num)
{
	if (! logger_) {
		logger_ = new LELogger();
		if (logger_) {
			int level = get_debug_level();
			loglevel_t level_le = INFO;
			switch(level) {
				case 1:
					level_le = DEBUG0;
					break;
				case 2:
					level_le = DEBUG1;
					break;
				case 3:
					level_le = DEBUG2;
					break;
			}
			logger_->SetLogLevel(level_le);
			LTFSAdminBase::SetLogger(logger_);
		}
	}

	std::shared_ptr<LTFSAdminSession> s =
		std::shared_ptr<LTFSAdminSession>(new LTFSAdminSession(node_addr, port_num));

	if (s) {
		MSG_LOG(GLESM700I, node_addr.c_str(), port_num);
		try {
			s->Connect();
			s->SessionLogin();
			MSG_LOG(GLESM701I, node_addr.c_str(), port_num, s->get_fd());
		} catch (AdminLibException& e) {
			MSG_LOG(GLESM186E, node_addr.c_str());
			s = std::shared_ptr<LTFSAdminSession>();
		}
	}

	return s;
}

std::shared_ptr<LTFSAdminSession> LEControl::Reconnect(std::shared_ptr<LTFSAdminSession> s)
{
	if (s) {
		MSG_LOG(GLESM702I, s->get_server().c_str(), s->get_port());
		try {
			s->Connect();
			s->SessionLogin();
			MSG_LOG(GLESM703I, s->get_server().c_str(), s->get_port(),
					s->get_fd());
		} catch (AdminLibException& e) {
			MSG_LOG(GLESM186E, s->get_server().c_str());
			s = std::shared_ptr<LTFSAdminSession>();
		}
	}

	return s;
}

void LEControl::Disconnect(std::shared_ptr<LTFSAdminSession> s)
{
	if (s) {
		MSG_LOG(GLESM704I, s->get_server().c_str(), s->get_port(), s->get_fd());
		try {
			try {
				s->SessionLogout();
			} catch (AdminLibException& e) {
				MSG_LOG(GLESM187E); // LTFS logout failed
			}
			s->Disconnect();
			MSG_LOG(GLESM705I, s->get_server().c_str(), s->get_port());
		} catch ( AdminLibException& e ) {
			MSG_LOG(GLESM187E); // LTFS logout failed
		}
	}

	return;
}

std::shared_ptr<LTFSNode> LEControl::InventoryNode(std::shared_ptr<LTFSAdminSession> s)
{
	std::shared_ptr<LTFSNode> n = std::shared_ptr<LTFSNode>();

	if (s && s->is_alived()) {
		/*
		 *  Remove GLESM706I and GLESM707I because the scheduler calls this function so much periodically
		 *  It is a stupid implementation to check the every node status every 1 second but we don't have
		 *  enough time to correct it... Now just remove the messages to overflow the messages.
		 */
		//MSG_LOG(GLESM706I, "node", s->get_server().c_str(), s->get_port(), s->get_fd());
		try {
			list<std::shared_ptr <LTFSNode> > node_list;
			s->SessionInventory(node_list);

			if (node_list.size() == 1) {
				//MSG_LOG(GLESM707I, "node", s->get_server().c_str(), s->get_port(), s->get_fd());
				list<std::shared_ptr <LTFSNode> >::iterator it = node_list.begin();
				n = *it;
			} else
				MSG_LOG(GLESM708E, node_list.size(), s->get_server().c_str(), s->get_port(), s->get_fd());
		} catch ( AdminLibException& e ) {
			MSG_LOG(GLESM709E, "Inventory", "node", s->get_server().c_str(), s->get_port(), s->get_fd(), e.what());
			n = std::shared_ptr<LTFSNode>();
		}
	}

	return n;
}

std::shared_ptr<Drive> LEControl::InventoryDrive(string id,
											std::shared_ptr<LTFSAdminSession> s,
											bool force)
{
	std::shared_ptr<Drive> d = std::shared_ptr<Drive>();

	if (s && s->is_alived()) {
		string type = "drive (" + id + ")";
		MSG_LOG(GLESM706I, type.c_str(), s->get_server().c_str(), s->get_port(),
				s->get_fd());
		try {
			list<std::shared_ptr <Drive> > drive_list;
			s->SessionInventory(drive_list, id, force);

			/*
			 * FIXME (Atsushi Abe): currently LE does not support filter function
			 * against the drive object. So that get all drives and search the target linearly.
			 */
			list<std::shared_ptr<Drive> >::iterator it;
			for (it = drive_list.begin(); it != drive_list.end(); ++it) {
				try {
					if (id == (*it)->GetObjectID()) {
						d = (*it);
						MSG_LOG(GLESM707I, type.c_str(), s->get_server().c_str(), s->get_port(), s->get_fd());
					}
				} catch ( InternalError& ie ) {
					if (ie.GetID() != "031E") {
						throw(ie);
					}
					/* Just ignore 0-byte ID objects */
				}
			}
			if (!d)
				MSG_LOG(GLESM710W, type.c_str(), s->get_server().c_str(), s->get_port(), s->get_fd());
		} catch ( AdminLibException& e ) {
			MSG_LOG(GLESM709E, "Inventory", type.c_str(), s->get_server().c_str(), s->get_port(), s->get_fd(), e.what());
			d = std::shared_ptr<Drive>();
		}
	}

	return d;
}

int LEControl::InventoryDrive(list<std::shared_ptr<Drive> > &drives,
							  std::shared_ptr<LTFSAdminSession> s,
							  bool assigned_only, bool force)
{
	if (s && s->is_alived()) {
		try {
			if (assigned_only) {
				MSG_LOG(GLESM706I, "assigned drive", s->get_server().c_str(), s->get_port(), s->get_fd());
				s->SessionInventory(drives, "__ACTIVE_ONLY__", force);
				MSG_LOG(GLESM707I, "assigned drive", s->get_server().c_str(), s->get_port(), s->get_fd());
			} else {
				MSG_LOG(GLESM706I, "drive", s->get_server().c_str(), s->get_port(), s->get_fd());
				s->SessionInventory(drives, "", force);
				MSG_LOG(GLESM707I, "drive", s->get_server().c_str(), s->get_port(), s->get_fd());
			}
		} catch ( AdminLibException& e ) {
			MSG_LOG(GLESM709E, "Inventory", "drive", s->get_server().c_str(), s->get_port(), s->get_fd(), e.what());
			drives.clear();
			return -1;
		}

		return drives.size();
	}

	return -1;
}

int LEControl::AssignDrive(string serial, std::shared_ptr<LTFSAdminSession> s)
{
	int rc = -1;

	if (s && s->is_alived()) {
		string type = "drive (" + serial + ")";
		MSG_LOG(GLESM711I, type.c_str(), s->get_server().c_str(), s->get_port(), s->get_fd());
		try {
			std::shared_ptr<Drive> d = InventoryDrive(serial, s);
			if (!d) {
				/* Refresh inventory and retry */
				d = InventoryDrive(serial, s);
			}

			if (d) {
				d->Add();
				rc = 0;
				MSG_LOG(GLESM712I, type.c_str(), s->get_server().c_str(), s->get_port(), s->get_fd());
			} else
				MSG_LOG(GLESM710W, type.c_str(), s->get_server().c_str(),
						s->get_port(), s->get_fd());
		} catch ( AdminLibException& e ) {
			MSG_LOG(GLESM709E, "Assign", type.c_str(), s->get_server().c_str(), s->get_port(), s->get_fd(), e.what());
			rc = -1;
		}
	}

	return rc;
}

int LEControl::UnassignDrive(std::shared_ptr<Drive> drive)
{
	int rc = -1;

	if (drive) {
		LTFSAdminSession *s = drive->get_session();
		string type = "drive (" + drive->GetObjectID() + ")";
		MSG_LOG(GLESM713I, type.c_str(), s->get_server().c_str(), s->get_port(),
				s->get_fd());
		try {
			drive->Remove();
			rc = 0;
			MSG_LOG(GLESM714I, type.c_str(), s->get_server().c_str(), s->get_port(),
					s->get_fd());
		} catch ( AdminLibException& e ) {
			string msg_oob = e.GetOOBError();
			if (msg_oob == "LTFSI1090E") {
				MSG_LOG(GLESM715I, type.c_str(), s->get_server().c_str(), s->get_port(),
						s->get_fd());
				rc = 0;
			} else {
				MSG_LOG(GLESM709E, "Unassign", type.c_str(), s->get_server().c_str(),
						s->get_port(), s->get_fd(), e.what());
				rc = -1;
			}
		}
	}

	return rc;
}

std::shared_ptr<Cartridge> LEControl::InventoryCartridge(string id,
													std::shared_ptr<LTFSAdminSession> s,
													bool force)
{
	std::shared_ptr<Cartridge> c = std::shared_ptr<Cartridge>();

	if (s && s->is_alived()) {
		string type = "tape (" + id + ")";
		MSG_LOG(GLESM706I, type.c_str(), s->get_server().c_str(), s->get_port(), s->get_fd());
		try {
			list<std::shared_ptr <Cartridge> > cartridge_list;
			s->SessionInventory(cartridge_list, id);

			if (cartridge_list.size() == 1) {
				MSG_LOG(GLESM707I, type.c_str(), s->get_server().c_str(), s->get_port(), s->get_fd());
				list<std::shared_ptr <Cartridge> >::iterator it = cartridge_list.begin();
				c = *it;
			} else
				MSG_LOG(GLESM716E, s->get_server().c_str(), s->get_port(), s->get_fd());
		} catch ( AdminLibException& e ) {
			MSG_LOG(GLESM709E, "Inventory", type.c_str(), s->get_server().c_str(), s->get_port(), s->get_fd(), e.what());
			c = std::shared_ptr<Cartridge>();
		}
	}

	return c;
}

int LEControl::InventoryCartridge(list<std::shared_ptr<Cartridge> > &cartridges,
								  std::shared_ptr<LTFSAdminSession> s,
								  bool assigned_only, bool force)
{
	if (s && s->is_alived()) {
		try {
			if (assigned_only) {
				MSG_LOG(GLESM706I, "assigned tape", s->get_server().c_str(), s->get_port(), s->get_fd());
				s->SessionInventory(cartridges, "__ACTIVE_ONLY__", force);
				MSG_LOG(GLESM707I, "assigned tape", s->get_server().c_str(), s->get_port(), s->get_fd());
			} else {
				MSG_LOG(GLESM706I, "tape", s->get_server().c_str(), s->get_port(), s->get_fd());
				s->SessionInventory(cartridges, "", force);
				MSG_LOG(GLESM707I, "tape", s->get_server().c_str(), s->get_port(), s->get_fd());
			}
		} catch ( AdminLibException& e ) {
			MSG_LOG(GLESM709E, "Inventory", "tape", s->get_server().c_str(), s->get_port(), s->get_fd(), e.what());
			cartridges.clear();
			return -1;
		}

		return cartridges.size();
	}

	return -1;
}

int LEControl::AssignCartridge(string barcode, std::shared_ptr<LTFSAdminSession> s, string drive_serial)
{
	int rc = -1;

	if (s && s->is_alived()) {
		string type = "tape (" + barcode + ")";
		MSG_LOG(GLESM711I, type.c_str(), s->get_server().c_str(), s->get_port(), s->get_fd());
		try {
			std::shared_ptr<Cartridge> c = InventoryCartridge(barcode, s);
			if (!c) {
				/* Refresh inventory and retry */
				c = InventoryCartridge(barcode, s);
			}

			if (c) c->Add();

			/* Refresh inventory and mount if it is UNKNOWN status */
			c = InventoryCartridge(barcode, s);
			if (c) {
				if (drive_serial.length() &&
					StatusConv::get_cart_value(c->get_status()) == TAPE_STATUS_UNKNOWN ) {
					c->Mount(drive_serial);
					c->Unmount();
				}
				rc = 0;
				MSG_LOG(GLESM712I, type.c_str(), s->get_server().c_str(), s->get_port(), s->get_fd());
			} else
				MSG_LOG(GLESM710W, type.c_str(), s->get_server().c_str(), s->get_port(), s->get_fd());

		} catch ( AdminLibException& e ) {
			MSG_LOG(GLESM709E, "Assign", type.c_str(), s->get_server().c_str(), s->get_port(), s->get_fd(), e.what());
			rc = -1;
		}
	}

	return rc;
}

int LEControl::UnassignCartridge(std::shared_ptr<Cartridge> cartridge, bool keep_on_drive)
{
	int rc = -1;

	if (cartridge) {
		LTFSAdminSession *s = cartridge->get_session();
		if (s && s->is_alived()) {
			string type = "tape (" + cartridge->GetObjectID() + ")";
			MSG_LOG(GLESM713I, type.c_str(), s->get_server().c_str(), s->get_port(), s->get_fd());
			try {
				cartridge->Remove(true, keep_on_drive);
				rc = 0;
				MSG_LOG(GLESM714I, type.c_str(), s->get_server().c_str(), s->get_port(), s->get_fd());
			} catch ( AdminLibException& e ) {
				string msg_oob = e.GetOOBError();
				if (msg_oob == "LTFSI1090E") {
					MSG_LOG(GLESM715I, type.c_str(), s->get_server().c_str(), s->get_port(), s->get_fd());
					rc = 0;
				} else {
					MSG_LOG(GLESM709E, "Unassign", type.c_str(), s->get_server().c_str(), s->get_port(), s->get_fd(), e.what());
					rc = -1;
				}
			}
		}
	}

	return rc;
}

int LEControl::MountCartridge(std::shared_ptr<Cartridge> cartridge, string drive_serial)
{
	int rc = -1;
	LTFSAdminSession *s = cartridge->get_session();

	if (s && s->is_alived()) {
		try {
			string type = cartridge->GetObjectID();
			MSG_LOG(GLESM717I, type.c_str(), drive_serial.c_str(), s->get_server().c_str(), s->get_port(), s->get_fd());
			rc = cartridge->Mount(drive_serial);
			MSG_LOG(GLESM718I, type.c_str(), drive_serial.c_str(), s->get_server().c_str(), s->get_port(), s->get_fd());
		} catch (AdminLibException& e) {
			string type = cartridge->GetObjectID() + "(" + drive_serial + ")";
			MSG_LOG(GLESM709E, "Mount", type.c_str(), s->get_server().c_str(), s->get_port(), s->get_fd(), e.what());
			return -1;
		}
	}

	return rc;
}

int LEControl::UnmountCartridge(std::shared_ptr<Cartridge> cartridge)
{
	int rc = -1;
	string type = cartridge->GetObjectID();
	LTFSAdminSession *s = cartridge->get_session();

	if (s && s->is_alived()) {
		try {
			MSG_LOG(GLESM719I, type.c_str(), s->get_server().c_str(), s->get_port(), s->get_fd());
			rc = cartridge->Unmount();
			MSG_LOG(GLESM720I, type.c_str(), s->get_server().c_str(), s->get_port(), s->get_fd());
		} catch (AdminLibException& e) {
			MSG_LOG(GLESM709E, "Unmount", type.c_str(), s->get_server().c_str(), s->get_port(), s->get_fd(), e.what());
			return -1;
		}
	}

	return rc;
}

int LEControl::SyncCartridge(std::shared_ptr<Cartridge> cartridge)
{
	int rc = -1;
	string type = cartridge->GetObjectID();
	LTFSAdminSession *s = cartridge->get_session();

	if (s && s->is_alived()) {
		try {
			MSG_LOG(GLESM721I, type.c_str(), s->get_server().c_str(), s->get_port(), s->get_fd());
			rc = cartridge->Sync();
			MSG_LOG(GLESM722I, type.c_str(), s->get_server().c_str(), s->get_port(), s->get_fd());
		} catch (AdminLibException& e) {
			MSG_LOG(GLESM709E, "Sync", type.c_str(), s->get_server().c_str(), s->get_port(), s->get_fd(), e.what());
			return -1;
		}
	}

	return rc;
}

const unordered_map<string, int> LEControl::format_errors_ = boost::assign::map_list_of
	( string("LTFSI1136E"), -LTFSEE_ALREADY_FORMATTED)
	( string("LTFSI1125E"), -LTFSEE_WRITE_PROTECTED)
	( string("LTFSI1079E"), -LTFSEE_TAPE_STATE_ERR)
	( string("LTFSI1080E"), -LTFSEE_TAPE_STATE_ERR)
	( string("LTFSI1081E"), -LTFSEE_TAPE_STATE_ERR)
	( string("LTFSI1082E"), -LTFSEE_INACCESSIBLE)
	( string("LTFSI1083E"), -LTFSEE_TAPE_STATE_ERR)
	( string("LTFSI1084E"), -LTFSEE_TAPE_STATE_ERR)
	( string("LTFSI1085E"), -LTFSEE_TAPE_STATE_ERR)
	( string("LTFSI1086E"), -LTFSEE_TAPE_STATE_ERR)
	( string("LTFSI1087E"), -LTFSEE_TAPE_STATE_ERR)
	( string("LTFSI1088E"), -LTFSEE_TAPE_STATE_ERR)
	;

int LEControl::FormatCartridge(std::shared_ptr<Cartridge> cartridge, string drive_serial, uint8_t density_code, bool force)
{
	int rc = -1;

	string type = cartridge->GetObjectID();
	LTFSAdminSession *s = cartridge->get_session();

	if (s && s->is_alived()) {
		try {
			cartridge->Remove(true, true, true);
		} catch (AdminLibException& e) {
			// Just ignore and try to format
			MSG_LOG(GLESM723I, type.c_str(), drive_serial.c_str(), s->get_server().c_str(),
					s->get_port(), s->get_fd(), "format");
		}

		try {
			MSG_LOG(GLESM724I, type.c_str(), drive_serial.c_str(), s->get_server().c_str(), s->get_port(), s->get_fd());
			uint8_t actual_density = 0;
			switch (density_code) {
				case FORMAT_JAG5:
				case FORMAT_JAG5W:
				case FORMAT_JAG4:
				case FORMAT_JAG4W:
					actual_density = density_code;
					break;
				default:
					/* density case should be 0(zero) at formatting LTO cartridge */
					break;
			}
			rc = cartridge->Format(drive_serial, actual_density, force);
			MSG_LOG(GLESM725I, type.c_str(), drive_serial.c_str(), s->get_server().c_str(), s->get_port(), s->get_fd());
		} catch (AdminLibException& e) {
			int err = LTFSEE_GENERAL_ERROR;
			unordered_map<string, int>::const_iterator it;
			string msg_oob = e.GetOOBError();

			it = format_errors_.find(msg_oob);
			if (it != format_errors_.end())
				err = it->second;
			else
				MSG_LOG(GLESM709E, "Format", type.c_str(), s->get_server().c_str(),
						s->get_port(), s->get_fd(), e.what());

			return err;
		}
	}

	return rc;
}

int LEControl::CheckCartridge(std::shared_ptr<Cartridge> cartridge, string drive_serial, bool deep)
{
	int rc = -1;

	string type = cartridge->GetObjectID();
	LTFSAdminSession *s = cartridge->get_session();

	if (s && s->is_alived()) {
		try {
			cartridge->Remove(true, true, true);
		} catch (AdminLibException& e) {
			// Just ignore and try to check
			MSG_LOG(GLESM723I, type.c_str(), drive_serial.c_str(), s->get_server().c_str(),
					s->get_port(), s->get_fd(), "recover");
		}

		try {
			MSG_LOG(GLESM726I, type.c_str(), drive_serial.c_str(), s->get_server().c_str(), s->get_port(), s->get_fd());
			rc = cartridge->Check(drive_serial, deep);
			MSG_LOG(GLESM727I, type.c_str(), drive_serial.c_str(), s->get_server().c_str(), s->get_port(), s->get_fd());
		} catch (AdminLibException& e) {
			MSG_LOG(GLESM709E, "Recovery", type.c_str(), s->get_server().c_str(), s->get_port(), s->get_fd(), e.what());
			return -1;
		}
	}

	return rc;
}

int LEControl::MoveCartridge(std::shared_ptr<Cartridge> cartridge, ltfs_slot_t slot_type, string drive_serial)
{
	int rc = -1;

	string type = cartridge->GetObjectID();
	string dest;
	switch(slot_type) {
		case SLOT_HOME:
			dest = "home slot";
			break;
		case SLOT_IE:
			dest = "ie slot";
			break;
		case SLOT_DRIVE:
			dest = drive_serial;
			break;
		default:
			dest = "unspecified";
			break;
	}

	LTFSAdminSession *s = cartridge->get_session();

	if (s && s->is_alived()) {
		try {
			MSG_LOG(GLESM728I, type.c_str(), dest.c_str(), s->get_server().c_str(), s->get_port(), s->get_fd());
			if (slot_type != SLOT_DRIVE && drive_serial == "") {
				cartridge->Move(slot_type, drive_serial);
				rc = 0;
			} else if (slot_type == SLOT_DRIVE && drive_serial != "") {
				cartridge->Move(slot_type, drive_serial);
				rc = 0;
			}
			MSG_LOG(GLESM729I, type.c_str(), dest.c_str(), s->get_server().c_str(), s->get_port(), s->get_fd());
		} catch ( AdminLibException& e ) {
			MSG_LOG(GLESM709E, "Move", type.c_str(), s->get_server().c_str(), s->get_port(), s->get_fd(), e.what());
			return -1;
		}
	}

	return rc;
}
