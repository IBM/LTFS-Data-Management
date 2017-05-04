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

class SingleDrive : public Drive {
private:
	bool busy;
public:
	SingleDrive(Drive drive) : Drive(drive), busy(false) {}
	bool isBusy() { return busy; }
	//	Drive *getDrive() { return dynamic_cast<Drive*>(this); }
};


class SingleTape : public Cartridge {
private:
	unsigned long inProgress;
public:
	SingleTape(Cartridge tape) : Cartridge(tape), inProgress(0) {}
	//	std::shared_ptr<Cartridge> getTape() { return tape; }
	unsigned long getInProgress() { return inProgress; }
};


void printTapeInventory(std::list<std::shared_ptr<SingleTape>> mytps)

{
	for (std::shared_ptr<SingleTape> i : mytps) {
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
	std::list<std::shared_ptr<SingleDrive>> mydrvs;
	std::list<std::shared_ptr<Cartridge>> tapes;
	std::list<std::shared_ptr<SingleTape>> mytps;

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
		mydrvs.push_back(std::make_shared<SingleDrive>(SingleDrive(*i)));

	for (std::shared_ptr<SingleDrive> i : mydrvs) {
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
		std::cout << (i->isBusy() == false ? "not used" : "in use") << std::endl;
	}

	std::cout << std::endl;

	rc = LEControl::InventoryCartridge(tapes, sess);

	if ( rc == -1 ) {
		std::cout << "unable to perform a drive inventory" << std::endl;
		LEControl::Disconnect(sess);
		return 1;
	}

	for(std::shared_ptr<Cartridge> i : tapes)
		mytps.push_back(std::make_shared<SingleTape>(SingleTape(*i)));

	printTapeInventory(mytps);

	std::shared_ptr<SingleTape> tapeToMount, tapeToUnmount;

	for (std::shared_ptr<SingleTape> i : mytps) {
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
			*tapeToUnmount = *(LEControl::InventoryCartridge(tapeToUnmount->GetObjectID(), sess));
			std::cout << "mounting " << tapeToMount->GetObjectID() << std::endl;
			tapeToMount->Mount(driveID);
			*tapeToMount = *(LEControl::InventoryCartridge(tapeToMount->GetObjectID(), sess));

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
