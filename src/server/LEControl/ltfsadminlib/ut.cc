#include <iostream>
#include <boost/shared_ptr.hpp>

#include "LTFSAdminSession.h"
#include "AdminLibException.h"

#include "LTFSNode.h"
#include "Drive.h"
#include "Cartridge.h"

int main(int argc, char **argv)
{
	using namespace std;
	using namespace boost;
	using namespace ltfsadmin;

	/* Set global loglevel */
	LTFSAdminBase::SetLogLevel(DEBUG3);

	try {
		LTFSAdminSession *session = new LTFSAdminSession("localhost", 7600);

		cout << "Connecting...";
		session->Connect();
		cout << "Done" << endl;

		cout << "Logging in...";
		session->SessionLogin();
		cout << "Done" << endl;

#if 0
		cout << "Getting Node Inventory...";
		list<shared_ptr <LTFSObject> > n_list = session->SessionInventory(LTFS_OBJ_LTFS_NODE);
		cout << "Done" << endl;
		cout << "---" << endl;
		for (list<shared_ptr <LTFSObject> >::iterator it = n_list.begin(); it != n_list.end(); ++it) {
			shared_ptr<LTFSNode> node = shared_polymorphic_downcast<LTFSNode>(*it);
			unordered_map<string, string>attrs = node->GetAttributes();

			for (unordered_map<string, string>::iterator attr_it = attrs.begin(); attr_it != attrs.end(); ++attr_it) {
				cout << attr_it->first << ":" << attr_it->second << endl;
			}
			cout << "---" << endl;

			cout << node->get_node_name();
			cout << "---" << endl;

			cout << node->get_status();
			cout << "---" << endl;
		}

		cout << "Getting Drive Inventory...";
		list<shared_ptr <LTFSObject> > d_list = session->SessionInventory(LTFS_OBJ_DRIVE);
		cout << "Done" << endl;
		cout << "---" << endl;
		for (list<shared_ptr <LTFSObject> >::iterator it = d_list.begin(); it != d_list.end(); ++it) {
			shared_ptr<Drive> drv = shared_polymorphic_downcast<Drive>(*it);
			unordered_map<string, string>attrs =  drv->GetAttributes();

			for (unordered_map<string, string>::iterator attr_it = attrs.begin(); attr_it != attrs.end(); ++attr_it) {
				cout << attr_it->first << ":" << attr_it->second << endl;
			}
			cout << "---" << endl;

			cout << drv->get_product_id();
			cout << "---" << endl;

			cout << drv->get_status();
			cout << "---" << endl;

			cout << drv->get_slot();
			cout << "---" << endl;

			cout << drv->get_devname();
			cout << "---" << endl;

			cout << drv->get_vendor();
			cout << "---" << endl;
		}


		cout << "Getting Cartridge Inventory...";
		list<shared_ptr <LTFSObject> > c_list = session->SessionInventory(LTFS_OBJ_CARTRIDGE);
		cout << "Done" << endl;
		cout << "---" << endl;
		for (list<shared_ptr <LTFSObject> >::iterator it = c_list.begin(); it != c_list.end(); ++it) {
			shared_ptr<Cartridge> cart = shared_polymorphic_downcast<Cartridge>(*it);
			unordered_map<string, string>attrs =  cart->GetAttributes();

			for (unordered_map<string, string>::iterator attr_it = attrs.begin(); attr_it != attrs.end(); ++attr_it) {
				cout << attr_it->first << ":" << attr_it->second << endl;
			}
			cout << "---" << endl;

			cout << cart->get_home_slot();
			cout << "---" << endl;

			cout << cart->get_slot();
			cout << "---" << endl;

			cout << cart->get_slot_type();
			cout << "---" << endl;

			cout << cart->get_total_cap();
			cout << "---" << endl;

			cout << cart->get_remaining_cap();
			cout << "---" << endl;

			cout << cart->get_status();
			cout << "---" << endl;

			cout << cart->get_placed_by_operator();
			cout << "---" << endl;
		}

		cout << "Getting Cartridge Inventory (Filtered) ...";
		list<shared_ptr <LTFSObject> > f_list = session->SessionInventory(LTFS_OBJ_CARTRIDGE, "YAM064L5");
		cout << "Done" << endl;
		cout << "---" << endl;
		for (list<shared_ptr <LTFSObject> >::iterator it = f_list.begin(); it != f_list.end(); ++it) {
			shared_ptr<Cartridge> cart = shared_polymorphic_downcast<Cartridge>(*it);
			unordered_map<string, string>attrs =  cart->GetAttributes();

			for (unordered_map<string, string>::iterator attr_it = attrs.begin(); attr_it != attrs.end(); ++attr_it) {
				cout << attr_it->first << ":" << attr_it->second << endl;
			}
			cout << "---" << endl;

			cart->Add();
			cart->Move(SLOT_IE);
			cart->Move(SLOT_HOME);
			cart->Add();
		}
#endif


		cout << "Logging out...";
		session->SessionLogout();
		cout << "Done" << endl;

		session->Disconnect();


	} catch (AdminLibException& e) {
		cerr << "Exception!!" << endl;
		cerr << e.what() << endl;
		return 1;
	}

	cout << "Succesfully Done" << endl;

	return 0;
}
