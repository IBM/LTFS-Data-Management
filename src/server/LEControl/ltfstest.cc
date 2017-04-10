#include <iostream>

#include <memory>
#include <list>
#include <condition_variable>
#include <unordered_map>
#include <thread>
#include <vector>
#include <set>

#include "LEControl.h"

int main(int argc, char **argv)

{
	LEControl lec;
	std::list<std::shared_ptr<Drive> > drives;
	std::list<std::shared_ptr<Cartridge> > tapes;
	int rc;

	std::shared_ptr<LTFSAdminSession> sess = lec.Connect("127.0.0.1", 7600);

	if ( !sess ) {
		std::cout << "unable to connect" << std::endl;
		return 1;
	}

	rc = lec.InventoryDrive(drives, sess);

	if ( rc == -1 ) {
		std::cout << "unable to perform a drive inventory" << std::endl;
		lec.Disconnect(sess);
		return 1;
	}

	for (auto i : drives) {
		std::cout << "id: "  << i->GetObjectID()
				  << ", devname: " << i->get_devname()
				  << ", slot: " << i->get_slot()
				  << ", status: " << i->get_status() << std::endl;
	}

	std::cout << std::endl;

	rc = lec.InventoryCartridge(tapes, sess);

	if ( rc == -1 ) {
		std::cout << "unable to perform a drive inventory" << std::endl;
		lec.Disconnect(sess);
		return 1;
	}

	for (auto i : tapes) {
		std::cout << "id: " << i->GetObjectID()
				  << ", slot: " << i->get_slot()
				  << ", total capacity: " << i->get_total_cap()
				  << ", remaining capacity: " << i->get_remaining_cap()
				  << ", status: " << i->get_status() << std::endl;
	}


	lec.Disconnect(sess);


	return 0;
}
