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

class SingleDrive {
private:
	std::shared_ptr<Drive> drive;
	bool busy;
public:
	SingleDrive(std::shared_ptr<Drive> _drive) : drive(_drive), busy(false) {}
	std::shared_ptr<Drive>  getDrive() { return drive; }
	bool isBusy() { return busy; }
};


class SingleTape {
private:
	std::shared_ptr<Cartridge> tape;
	unsigned long inProgress;
public:
	SingleTape(std::shared_ptr<Cartridge> _tape) : tape(_tape), inProgress(0) {}
	std::shared_ptr<Cartridge> getTape() { return tape; }
	unsigned long getInProgress() { return inProgress; }
};


void printTapeInventory(std::list<SingleTape> mytps)

{
	for (SingleTape i : mytps) {
		std::cout << "id: " << i.getTape()->GetObjectID()
				  << ", slot: " << i.getTape()->get_slot()
				  << ", total capacity: " << i.getTape()->get_total_cap()
				  << ", remaining capacity: " << i.getTape()->get_remaining_cap()
				  << ", status: " << i.getTape()->get_status() << std::endl;
	}
}

int main(int argc, char **argv)

{
	std::list<std::shared_ptr<Drive> > drives;
	std::list<SingleDrive> mydrvs;
	std::list<std::shared_ptr<Cartridge> > tapes;
	std::list<SingleTape> mytps;

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

	for (std::shared_ptr<Drive> i : drives)
		mydrvs.push_back(SingleDrive(i));

	for (SingleDrive i : mydrvs) {
		driveID = i.getDrive()->GetObjectID();
		slot = i.getDrive()->get_slot();
		std::cout << "id: "  << i.getDrive()->GetObjectID()
				  << ", product id: " << i.getDrive()->get_product_id()
				  << ", firmware revision: " << i.getDrive()->get_fw_revision()
				  << ", vendor: " << i.getDrive()->get_vendor()
				  << ", devname: " << i.getDrive()->get_devname()
				  << ", slot: " << i.getDrive()->get_slot()
				  << ", node: " << i.getDrive()->get_node()
				  << ", status: " << i.getDrive()->get_status() << std::endl;
		std::cout << (i.isBusy() == false ? "not used" : "in use") << std::endl;
	}

	std::cout << std::endl;

	rc = LEControl::InventoryCartridge(tapes, sess);

	if ( rc == -1 ) {
		std::cout << "unable to perform a drive inventory" << std::endl;
		LEControl::Disconnect(sess);
		return 1;
	}

	for(std::shared_ptr<Cartridge> i : tapes)
		mytps.push_back(SingleTape(i));

	printTapeInventory(mytps);

	std::shared_ptr<Cartridge> tapeToMount, tapeToUnmount;

	for (SingleTape i : mytps) {
		if ( tapeID.compare(i.getTape()->GetObjectID()) == 0 )
			tapeToMount = i.getTape();
		if ( i.getTape()->get_slot() == slot )
			tapeToUnmount = i.getTape();
	}

	if ( justList == false ) {
		if ( slot == tapeToMount->get_slot() ) {
			std::cout << "tape " << tapeID << " is already mounted" << std::endl;
		}
		else {
			std::cout << "unmounting " << tapeToUnmount->GetObjectID() << std::endl;
			tapeToUnmount->Unmount();
			*tapeToUnmount = *(LEControl::InventoryCartridge(tapeToUnmount->GetObjectID(), sess));
			std::cout << "mounting " << tapeToMount->GetObjectID() << std::endl;
			tapeToMount->Mount(driveID);
			*tapeToMount = *(LEControl::InventoryCartridge(tapeToMount->GetObjectID(), sess));
			//rc = LEControl::InventoryCartridge(tapes, sess);

			if ( rc == -1 ) {
				std::cout << "unable to perform a drive inventory" << std::endl;
				LEControl::Disconnect(sess);
				return 1;
			}

			printTapeInventory(mytps);
		}
	}


	LEControl::Disconnect(sess);


	return 0;
}
