#include <iostream>

#include <memory>
#include <list>
#include <condition_variable>
#include <unordered_map>
#include <thread>
#include <vector>
#include <set>

#include "LEControl.h"
#include "ltfsadminlib/Cartridge.h"

using namespace ltfsadmin;

void printTapeInventory(std::shared_ptr<LTFSAdminSession> sess)

{
	std::list<std::shared_ptr<Cartridge> > tapes;
	int rc;

	rc = LEControl::InventoryCartridge(tapes, sess);

	if ( rc == -1 ) {
		std::cout << "unable to perform a drive inventory" << std::endl;
		return;
	}

	for (auto i : tapes) {
		std::cout << "id: " << i->GetObjectID()
				  << ", slot: " << i->get_slot()
				  << ", total capacity: " << i->get_total_cap()
				  << ", remaining capacity: " << i->get_remaining_cap()
				  << ", status: " << i->get_status() << std::endl;
	}
}

int main(int argc, char **argv)

{
	std::list<std::shared_ptr<Drive> > drives;
	std::list<std::shared_ptr<Cartridge> > tapes;

	std::string tapeID;
	std::string driveID;
	uint16_t slot;
	bool justList = true;
	int rc;

	if ( argc == 2 ) {
		tapeID = std::string(argv[1]);
		justList = false;
	}

	std::shared_ptr<LTFSAdminSession> sess = LEControl::Connect("127.0.0.1", 7600);


	if ( !sess ) {
		std::cout << "unable to connect" << std::endl;
		return 1;
	}

	rc = LEControl::InventoryDrive(drives, sess);

	if ( rc == -1 ) {
		std::cout << "unable to perform a drive inventory" << std::endl;
		LEControl::Disconnect(sess);
		return 1;
	}

	for (auto i : drives) {
		driveID = i->GetObjectID();
		slot = i->get_slot();
		std::cout << "id: "  << i->GetObjectID()
				  << ", product id: " << i->get_product_id()
				  << ", firmware revision: " << i->get_fw_revision()
				  << ", vendor: " << i->get_vendor()
				  << ", devname: " << i->get_devname()
				  << ", slot: " << i->get_slot()
				  << ", node: " << i->get_node()
				  << ", status: " << i->get_status() << std::endl;
	}

	std::cout << std::endl;

	rc = LEControl::InventoryCartridge(tapes, sess);

	if ( rc == -1 ) {
		std::cout << "unable to perform a drive inventory" << std::endl;
		LEControl::Disconnect(sess);
		return 1;
	}

	printTapeInventory(sess);

	std::shared_ptr<Cartridge> tapeToMount, tapeToUnmount;

	for (auto i : tapes) {
		if ( tapeID.compare(i->GetObjectID()) == 0 )
			tapeToMount = i;
		if ( i->get_slot() == slot )
			tapeToUnmount = i;
	}

	if ( justList == false ) {
		if ( slot == tapeToMount->get_slot() ) {
			std::cout << "tape " << tapeID << " is already mounted" << std::endl;
		}
		else {
			std::cout << "unmounting " << tapeToUnmount->GetObjectID() << std::endl;
			tapeToUnmount->Unmount();
			std::cout << "mounting " << tapeToMount->GetObjectID() << std::endl;
			tapeToMount->Mount(driveID);
			printTapeInventory(sess);
		}
	}


	LEControl::Disconnect(sess);


	return 0;
}
