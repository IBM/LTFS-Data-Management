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

class OpenLTFSDrive : public Drive {
private:
	bool busy;
public:
	OpenLTFSDrive(Drive drive) : Drive(drive), busy(false) {}
	bool isBusy() { return busy; }
	//	Drive *getDrive() { return dynamic_cast<Drive*>(this); }
};


class OpenLTFSCartridge : public Cartridge {
private:
	unsigned long inProgress;
public:
	OpenLTFSCartridge(Cartridge cartridge) : Cartridge(cartridge), inProgress(0) {}
	//	std::shared_ptr<Cartridge> getCartridge() { return cartridge; }
	unsigned long getInProgress() { return inProgress; }
};


void printCartridgeInventory(std::list<std::shared_ptr<OpenLTFSCartridge>> mytps)

{
	for (std::shared_ptr<OpenLTFSCartridge> i : mytps) {
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
	std::list<std::shared_ptr<OpenLTFSDrive>> mydrvs;
	std::list<std::shared_ptr<Cartridge>> cartridges;
	std::list<std::shared_ptr<OpenLTFSCartridge>> mytps;

	std::string cartridgeID;
	std::string driveID;
	uint16_t slot;
	bool justList = true;
	int rc;

	if ( argc == 2 ) {
		cartridgeID = std::string(argv[1]);
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
		mydrvs.push_back(std::make_shared<OpenLTFSDrive>(OpenLTFSDrive(*i)));

	for (std::shared_ptr<OpenLTFSDrive> i : mydrvs) {
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

	rc = LEControl::InventoryCartridge(cartridges, sess);

	if ( rc == -1 ) {
		std::cout << "unable to perform a drive inventory" << std::endl;
		LEControl::Disconnect(sess);
		return 1;
	}

	for(std::shared_ptr<Cartridge> i : cartridges)
		mytps.push_back(std::make_shared<OpenLTFSCartridge>(OpenLTFSCartridge(*i)));

	printCartridgeInventory(mytps);

	std::shared_ptr<OpenLTFSCartridge> cartridgeToMount, cartridgeToUnmount;

	for (std::shared_ptr<OpenLTFSCartridge> i : mytps) {
		if ( cartridgeID.compare(i->GetObjectID()) == 0 )
			cartridgeToMount = i;
		if ( i->get_slot() == slot )
			cartridgeToUnmount = i;
	}

	if ( justList == false ) {
		if ( slot == cartridgeToMount->get_slot() ) {
			std::cout << "cartridge " << cartridgeID << " is already mounted" << std::endl;
		}
		else {
			std::cout << "unmounting " << cartridgeToUnmount->GetObjectID() << std::endl;
			cartridgeToUnmount->Unmount();
			*cartridgeToUnmount = *(LEControl::InventoryCartridge(cartridgeToUnmount->GetObjectID(), sess));
			std::cout << "mounting " << cartridgeToMount->GetObjectID() << std::endl;
			cartridgeToMount->Mount(driveID);
			*cartridgeToMount = *(LEControl::InventoryCartridge(cartridgeToMount->GetObjectID(), sess));

			if ( rc == -1 ) {
				std::cout << "unable to perform a drive inventory" << std::endl;
				LEControl::Disconnect(sess);
				return 1;
			}

			printCartridgeInventory(mytps);
		}
	}


	LEControl::Disconnect(sess);


	return 0;
}
