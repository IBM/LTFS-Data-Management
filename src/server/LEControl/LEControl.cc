#include <sys/resource.h>

#include <memory>
#include <list>
#include <condition_variable>
#include <unordered_map>
#include <thread>
#include <vector>
#include <set>

#include "src/common/util/util.h"
#include "src/common/messages/Message.h"
#include "src/common/tracing/Trace.h"
#include "src/common/errors/errors.h"
#include "src/common/const/Const.h"

#include "LEControl.h"
#include "StatusConv.h"

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

std::shared_ptr<LTFSAdminSession> LEControl::Connect(std::string node_addr, uint16_t port_num)
{
	std::shared_ptr<LTFSAdminSession> s =
		std::shared_ptr<LTFSAdminSession>(new LTFSAdminSession(node_addr, port_num));

	if (s) {
		MSG(LTFSDML0700I, node_addr.c_str(), port_num);
		try {
			s->Connect();
			s->SessionLogin();
			MSG(LTFSDML0701I, node_addr.c_str(), port_num, s->get_fd());
		} catch (AdminLibException& e) {
			MSG(LTFSDML0186E, node_addr.c_str());
			s = std::shared_ptr<LTFSAdminSession>();
		}
	}

	return s;
}

std::shared_ptr<LTFSAdminSession> LEControl::Reconnect(std::shared_ptr<LTFSAdminSession> s)
{
	if (s) {
		MSG(LTFSDML0702I, s->get_server().c_str(), s->get_port());
		try {
			s->Connect();
			s->SessionLogin();
			MSG(LTFSDML0703I, s->get_server().c_str(), s->get_port(),
					s->get_fd());
		} catch (AdminLibException& e) {
			MSG(LTFSDML0186E, s->get_server().c_str());
			s = std::shared_ptr<LTFSAdminSession>();
		}
	}

	return s;
}

void LEControl::Disconnect(std::shared_ptr<LTFSAdminSession> s)
{
	if (s) {
		MSG(LTFSDML0704I, s->get_server().c_str(), s->get_port(), s->get_fd());
		try {
			try {
				s->SessionLogout();
			} catch (AdminLibException& e) {
				MSG(LTFSDML0187E); // LTFS logout failed
			}
			s->Disconnect();
			MSG(LTFSDML0705I, s->get_server().c_str(), s->get_port());
		} catch ( AdminLibException& e ) {
			MSG(LTFSDML0187E); // LTFS logout failed
		}
	}

	return;
}

std::shared_ptr<LTFSNode> LEControl::InventoryNode(std::shared_ptr<LTFSAdminSession> s)
{
	std::shared_ptr<LTFSNode> n = std::shared_ptr<LTFSNode>();

	if (s && s->is_alived()) {
		/*
		 *  Remove LTFSDML0706I and LTFSDML0707I because the scheduler calls this function so much periodically
		 *  It is a stupid implementation to check the every node status every 1 second but we don't have
		 *  enough time to correct it... Now just remove the messages to overflow the messages.
		 */
		//MSG(LTFSDML0706I, "node", s->get_server().c_str(), s->get_port(), s->get_fd());
		try {
			std::list<std::shared_ptr <LTFSNode> > node_list;
			s->SessionInventory(node_list);

			if (node_list.size() == 1) {
				//MSG(LTFSDML0707I, "node", s->get_server().c_str(), s->get_port(), s->get_fd());
				std::list<std::shared_ptr <LTFSNode> >::iterator it = node_list.begin();
				n = *it;
			} else
				MSG(LTFSDML0708E, node_list.size(), s->get_server().c_str(), s->get_port(), s->get_fd());
		} catch ( AdminLibException& e ) {
			MSG(LTFSDML0709E, "Inventory", "node", s->get_server().c_str(), s->get_port(), s->get_fd(), e.what());
			n = std::shared_ptr<LTFSNode>();
		}
	}

	return n;
}

std::shared_ptr<Drive> LEControl::InventoryDrive(std::string id,
											std::shared_ptr<LTFSAdminSession> s,
											bool force)
{
	std::shared_ptr<Drive> d = std::shared_ptr<Drive>();

	if (s && s->is_alived()) {
		std::string type = "drive (" + id + ")";
		MSG(LTFSDML0706I, type.c_str(), s->get_server().c_str(), s->get_port(),
				s->get_fd());
		try {
			std::list<std::shared_ptr <Drive> > drive_list;
			s->SessionInventory(drive_list, id, force);

			/*
			 * FIXME (Atsushi Abe): currently LE does not support filter function
			 * against the drive object. So that get all drives and search the target linearly.
			 */
			std::list<std::shared_ptr<Drive> >::iterator it;
			for (it = drive_list.begin(); it != drive_list.end(); ++it) {
				try {
					if (id == (*it)->GetObjectID()) {
						d = (*it);
						MSG(LTFSDML0707I, type.c_str(), s->get_server().c_str(), s->get_port(), s->get_fd());
					}
				} catch ( InternalError& ie ) {
					if (ie.GetID() != "031E") {
						throw(ie);
					}
					/* Just ignore 0-byte ID objects */
				}
			}
			if (!d)
				MSG(LTFSDML0710W, type.c_str(), s->get_server().c_str(), s->get_port(), s->get_fd());
		} catch ( AdminLibException& e ) {
			MSG(LTFSDML0709E, "Inventory", type.c_str(), s->get_server().c_str(), s->get_port(), s->get_fd(), e.what());
			d = std::shared_ptr<Drive>();
		}
	}

	return d;
}

int LEControl::InventoryDrive(std::list<std::shared_ptr<Drive> > &drives,
							  std::shared_ptr<LTFSAdminSession> s,
							  bool assigned_only, bool force)
{
	if (s && s->is_alived()) {
		try {
			if (assigned_only) {
				MSG(LTFSDML0706I, "assigned drive", s->get_server().c_str(), s->get_port(), s->get_fd());
				s->SessionInventory(drives, "__ACTIVE_ONLY__", force);
				MSG(LTFSDML0707I, "assigned drive", s->get_server().c_str(), s->get_port(), s->get_fd());
			} else {
				MSG(LTFSDML0706I, "drive", s->get_server().c_str(), s->get_port(), s->get_fd());
				s->SessionInventory(drives, "", force);
				MSG(LTFSDML0707I, "drive", s->get_server().c_str(), s->get_port(), s->get_fd());
			}
		} catch ( AdminLibException& e ) {
			MSG(LTFSDML0709E, "Inventory", "drive", s->get_server().c_str(), s->get_port(), s->get_fd(), e.what());
			drives.clear();
			return -1;
		}

		return drives.size();
	}

	return -1;
}

int LEControl::AssignDrive(std::string serial, std::shared_ptr<LTFSAdminSession> s)
{
	int rc = -1;

	if (s && s->is_alived()) {
		std::string type = "drive (" + serial + ")";
		MSG(LTFSDML0711I, type.c_str(), s->get_server().c_str(), s->get_port(), s->get_fd());
		try {
			std::shared_ptr<Drive> d = InventoryDrive(serial, s);
			if (!d) {
				/* Refresh inventory and retry */
				d = InventoryDrive(serial, s);
			}

			if (d) {
				d->Add();
				rc = 0;
				MSG(LTFSDML0712I, type.c_str(), s->get_server().c_str(), s->get_port(), s->get_fd());
			} else
				MSG(LTFSDML0710W, type.c_str(), s->get_server().c_str(),
						s->get_port(), s->get_fd());
		} catch ( AdminLibException& e ) {
			MSG(LTFSDML0709E, "Assign", type.c_str(), s->get_server().c_str(), s->get_port(), s->get_fd(), e.what());
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
		std::string type = "drive (" + drive->GetObjectID() + ")";
		MSG(LTFSDML0713I, type.c_str(), s->get_server().c_str(), s->get_port(),
				s->get_fd());
		try {
			drive->Remove();
			rc = 0;
			MSG(LTFSDML0714I, type.c_str(), s->get_server().c_str(), s->get_port(),
					s->get_fd());
		} catch ( AdminLibException& e ) {
			std::string msg_oob = e.GetOOBError();
			if (msg_oob == "LTFSI1090E") {
				MSG(LTFSDML0715I, type.c_str(), s->get_server().c_str(), s->get_port(),
						s->get_fd());
				rc = 0;
			} else {
				MSG(LTFSDML0709E, "Unassign", type.c_str(), s->get_server().c_str(),
						s->get_port(), s->get_fd(), e.what());
				rc = -1;
			}
		}
	}

	return rc;
}

std::shared_ptr<Cartridge> LEControl::InventoryCartridge(std::string id,
													std::shared_ptr<LTFSAdminSession> s,
													bool force)
{
	std::shared_ptr<Cartridge> c = std::shared_ptr<Cartridge>();

	if (s && s->is_alived()) {
		std::string type = "tape (" + id + ")";
		MSG(LTFSDML0706I, type.c_str(), s->get_server().c_str(), s->get_port(), s->get_fd());
		try {
			std::list<std::shared_ptr <Cartridge> > cartridge_list;
			s->SessionInventory(cartridge_list, id);

			if (cartridge_list.size() == 1) {
				MSG(LTFSDML0707I, type.c_str(), s->get_server().c_str(), s->get_port(), s->get_fd());
				std::list<std::shared_ptr <Cartridge> >::iterator it = cartridge_list.begin();
				c = *it;
			} else
				MSG(LTFSDML0716E, s->get_server().c_str(), s->get_port(), s->get_fd());
		} catch ( AdminLibException& e ) {
			MSG(LTFSDML0709E, "Inventory", type.c_str(), s->get_server().c_str(), s->get_port(), s->get_fd(), e.what());
			c = std::shared_ptr<Cartridge>();
		}
	}

	return c;
}

int LEControl::InventoryCartridge(std::list<std::shared_ptr<Cartridge> > &cartridges,
								  std::shared_ptr<LTFSAdminSession> s,
								  bool assigned_only, bool force)
{
	if (s && s->is_alived()) {
		try {
			if (assigned_only) {
				MSG(LTFSDML0706I, "assigned tape", s->get_server().c_str(), s->get_port(), s->get_fd());
				s->SessionInventory(cartridges, "__ACTIVE_ONLY__", force);
				MSG(LTFSDML0707I, "assigned tape", s->get_server().c_str(), s->get_port(), s->get_fd());
			} else {
				MSG(LTFSDML0706I, "tape", s->get_server().c_str(), s->get_port(), s->get_fd());
				s->SessionInventory(cartridges, "", force);
				MSG(LTFSDML0707I, "tape", s->get_server().c_str(), s->get_port(), s->get_fd());
			}
		} catch ( AdminLibException& e ) {
			MSG(LTFSDML0709E, "Inventory", "tape", s->get_server().c_str(), s->get_port(), s->get_fd(), e.what());
			cartridges.clear();
			return -1;
		}

		return cartridges.size();
	}

	return -1;
}

int LEControl::AssignCartridge(std::string barcode, std::shared_ptr<LTFSAdminSession> s, std::string drive_serial)
{
	int rc = -1;

	if (s && s->is_alived()) {
		std::string type = "tape (" + barcode + ")";
		MSG(LTFSDML0711I, type.c_str(), s->get_server().c_str(), s->get_port(), s->get_fd());
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
				MSG(LTFSDML0712I, type.c_str(), s->get_server().c_str(), s->get_port(), s->get_fd());
			} else
				MSG(LTFSDML0710W, type.c_str(), s->get_server().c_str(), s->get_port(), s->get_fd());

		} catch ( AdminLibException& e ) {
			MSG(LTFSDML0709E, "Assign", type.c_str(), s->get_server().c_str(), s->get_port(), s->get_fd(), e.what());
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
			std::string type = "tape (" + cartridge->GetObjectID() + ")";
			MSG(LTFSDML0713I, type.c_str(), s->get_server().c_str(), s->get_port(), s->get_fd());
			try {
				cartridge->Remove(true, keep_on_drive);
				rc = 0;
				MSG(LTFSDML0714I, type.c_str(), s->get_server().c_str(), s->get_port(), s->get_fd());
			} catch ( AdminLibException& e ) {
				std::string msg_oob = e.GetOOBError();
				if (msg_oob == "LTFSI1090E") {
					MSG(LTFSDML0715I, type.c_str(), s->get_server().c_str(), s->get_port(), s->get_fd());
					rc = 0;
				} else {
					MSG(LTFSDML0709E, "Unassign", type.c_str(), s->get_server().c_str(), s->get_port(), s->get_fd(), e.what());
					rc = -1;
				}
			}
		}
	}

	return rc;
}

int LEControl::MountCartridge(std::shared_ptr<Cartridge> cartridge, std::string drive_serial)
{
	int rc = -1;
	LTFSAdminSession *s = cartridge->get_session();

	if (s && s->is_alived()) {
		try {
			std::string type = cartridge->GetObjectID();
			MSG(LTFSDML0717I, type.c_str(), drive_serial.c_str(), s->get_server().c_str(), s->get_port(), s->get_fd());
			rc = cartridge->Mount(drive_serial);
			MSG(LTFSDML0718I, type.c_str(), drive_serial.c_str(), s->get_server().c_str(), s->get_port(), s->get_fd());
		} catch (AdminLibException& e) {
			std::string type = cartridge->GetObjectID() + "(" + drive_serial + ")";
			MSG(LTFSDML0709E, "Mount", type.c_str(), s->get_server().c_str(), s->get_port(), s->get_fd(), e.what());
			return -1;
		}
	}

	return rc;
}

int LEControl::UnmountCartridge(std::shared_ptr<Cartridge> cartridge)
{
	int rc = -1;
	std::string type = cartridge->GetObjectID();
	LTFSAdminSession *s = cartridge->get_session();

	if (s && s->is_alived()) {
		try {
			MSG(LTFSDML0719I, type.c_str(), s->get_server().c_str(), s->get_port(), s->get_fd());
			rc = cartridge->Unmount();
			MSG(LTFSDML0720I, type.c_str(), s->get_server().c_str(), s->get_port(), s->get_fd());
		} catch (AdminLibException& e) {
			MSG(LTFSDML0709E, "Unmount", type.c_str(), s->get_server().c_str(), s->get_port(), s->get_fd(), e.what());
			return -1;
		}
	}

	return rc;
}

int LEControl::SyncCartridge(std::shared_ptr<Cartridge> cartridge)
{
	int rc = -1;
	std::string type = cartridge->GetObjectID();
	LTFSAdminSession *s = cartridge->get_session();

	if (s && s->is_alived()) {
		try {
			MSG(LTFSDML0721I, type.c_str(), s->get_server().c_str(), s->get_port(), s->get_fd());
			rc = cartridge->Sync();
			MSG(LTFSDML0722I, type.c_str(), s->get_server().c_str(), s->get_port(), s->get_fd());
		} catch (AdminLibException& e) {
			MSG(LTFSDML0709E, "Sync", type.c_str(), s->get_server().c_str(), s->get_port(), s->get_fd(), e.what());
			return -1;
		}
	}

	return rc;
}

const std::unordered_map<std::string, int> LEControl::format_errors_ = {
	{ std::string("LTFSI1136E"), -static_cast<int>(Error::LTFSDM_ALREADY_FORMATTED) },
	{ std::string("LTFSI1125E"), -static_cast<int>(Error::LTFSDM_WRITE_PROTECTED) },
	{ std::string("LTFSI1079E"), -static_cast<int>(Error::LTFSDM_TAPE_STATE_ERR) },
	{ std::string("LTFSI1080E"), -static_cast<int>(Error::LTFSDM_TAPE_STATE_ERR) },
	{ std::string("LTFSI1081E"), -static_cast<int>(Error::LTFSDM_TAPE_STATE_ERR) },
	{ std::string("LTFSI1082E"), -static_cast<int>(Error::LTFSDM_INACCESSIBLE) },
	{ std::string("LTFSI1083E"), -static_cast<int>(Error::LTFSDM_TAPE_STATE_ERR) },
	{ std::string("LTFSI1084E"), -static_cast<int>(Error::LTFSDM_TAPE_STATE_ERR) },
	{ std::string("LTFSI1085E"), -static_cast<int>(Error::LTFSDM_TAPE_STATE_ERR) },
	{ std::string("LTFSI1086E"), -static_cast<int>(Error::LTFSDM_TAPE_STATE_ERR) },
	{ std::string("LTFSI1087E"), -static_cast<int>(Error::LTFSDM_TAPE_STATE_ERR) },
	{ std::string("LTFSI1088E"), -static_cast<int>(Error::LTFSDM_TAPE_STATE_ERR) }};


int LEControl::FormatCartridge(std::shared_ptr<Cartridge> cartridge, std::string drive_serial, uint8_t density_code, bool force)
{
	int rc = -1;

	std::string type = cartridge->GetObjectID();
	LTFSAdminSession *s = cartridge->get_session();

	if (s && s->is_alived()) {
		try {
			cartridge->Remove(true, true, true);
		} catch (AdminLibException& e) {
			// Just ignore and try to format
			MSG(LTFSDML0723I, type.c_str(), drive_serial.c_str(), s->get_server().c_str(),
					s->get_port(), s->get_fd(), "format");
		}

		try {
			MSG(LTFSDML0724I, type.c_str(), drive_serial.c_str(), s->get_server().c_str(), s->get_port(), s->get_fd());
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
			MSG(LTFSDML0725I, type.c_str(), drive_serial.c_str(), s->get_server().c_str(), s->get_port(), s->get_fd());
		} catch (AdminLibException& e) {
			int err = static_cast<int>(Error::LTFSDM_GENERAL_ERROR);
			std::unordered_map<std::string, int>::const_iterator it;
			std::string msg_oob = e.GetOOBError();

			it = format_errors_.find(msg_oob);
			if (it != format_errors_.end())
				err = it->second;
			else
				MSG(LTFSDML0709E, "Format", type.c_str(), s->get_server().c_str(),
						s->get_port(), s->get_fd(), e.what());

			return err;
		}
	}

	return rc;
}

int LEControl::CheckCartridge(std::shared_ptr<Cartridge> cartridge, std::string drive_serial, bool deep)
{
	int rc = -1;

	std::string type = cartridge->GetObjectID();
	LTFSAdminSession *s = cartridge->get_session();

	if (s && s->is_alived()) {
		try {
			cartridge->Remove(true, true, true);
		} catch (AdminLibException& e) {
			// Just ignore and try to check
			MSG(LTFSDML0723I, type.c_str(), drive_serial.c_str(), s->get_server().c_str(),
					s->get_port(), s->get_fd(), "recover");
		}

		try {
			MSG(LTFSDML0726I, type.c_str(), drive_serial.c_str(), s->get_server().c_str(), s->get_port(), s->get_fd());
			rc = cartridge->Check(drive_serial, deep);
			MSG(LTFSDML0727I, type.c_str(), drive_serial.c_str(), s->get_server().c_str(), s->get_port(), s->get_fd());
		} catch (AdminLibException& e) {
			MSG(LTFSDML0709E, "Recovery", type.c_str(), s->get_server().c_str(), s->get_port(), s->get_fd(), e.what());
			return -1;
		}
	}

	return rc;
}

int LEControl::MoveCartridge(std::shared_ptr<Cartridge> cartridge, ltfs_slot_t slot_type, std::string drive_serial)
{
	int rc = -1;

	std::string type = cartridge->GetObjectID();
	std::string dest;
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
			MSG(LTFSDML0728I, type.c_str(), dest.c_str(), s->get_server().c_str(), s->get_port(), s->get_fd());
			if (slot_type != SLOT_DRIVE && drive_serial == "") {
				cartridge->Move(slot_type, drive_serial);
				rc = 0;
			} else if (slot_type == SLOT_DRIVE && drive_serial != "") {
				cartridge->Move(slot_type, drive_serial);
				rc = 0;
			}
			MSG(LTFSDML0729I, type.c_str(), dest.c_str(), s->get_server().c_str(), s->get_port(), s->get_fd());
		} catch ( AdminLibException& e ) {
			MSG(LTFSDML0709E, "Move", type.c_str(), s->get_server().c_str(), s->get_port(), s->get_fd(), e.what());
			return -1;
		}
	}

	return rc;
}
