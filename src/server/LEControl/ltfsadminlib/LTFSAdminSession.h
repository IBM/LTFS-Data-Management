#pragma once

#include <string>

#include <stdint.h>                /* Shall be replaced to cstdint in C++11 */
////#include <boost/std::shared_ptr.hpp>    /* Shall be replaced to shared in C++11 */
////#include <boost/unordered_map.hpp> /* Shall be replaced to unordered_map in C++11 */
////#include <boost/thread.hpp>        /* Shall be replaced to thread in C++11 */

#include <unordered_map>

#include <mutex>

#include "LTFSAdminBase.h"
#include "LTFSRequestMessage.h"
#include "LTFSResponseMessage.h"
#include "LTFSObject.h"

namespace ltfsadmin {

class Result;
class Action;
class LTFSNode;
class Cartridge;
class Drive;

/** Session class for LTFS admin channel (OOBv2)
 *
 *  This class provides the functionality of handling OOBv2 message passing. This class manages socket connection, sequence number
 *  and a response message against request message which was already sent out.
 */
class LTFSAdminSession : public LTFSAdminBase {
public:
	/** The constructor
	 *
	 *  @param server server name or ip addrees to be connected. server name shall be resolved by resolver
	 *  @param port port number to be connected
	 */
	LTFSAdminSession(std::string server, int port);

	/** The destructor
	 */
	virtual ~LTFSAdminSession();

	/** Connect to specified server
	 *
	 *  Open a socket to specified server
     *
	 * @return 0 on success, otherwise error
	 */
	int Connect();

	/** Discnnect the socket
	 **
	 * @return 0 on success, otherwise error
	 */
	int Disconnect();

	/** Genarate new sequence number to ne used for next request
	 **
	 * @return sequence numebr. never fail.
	 */
	uint64_t GetSequence(void);

	/** Issue Login request
	 *
	 *  Throw an exception inheritated from AdminLibException when login is failed
	 *
	 *  @param user user name if authentication is required
	 *  @param password password if authentication is required
	 *  @return 0 on success, otherwise non-zero;
	 */
	int SessionLogin(std::string user = "", std::string password = "");

	/** Issue Logout request
	 *
	 *  Throw an exception inheritated from AdminLibException when loout is failed
	 *
	 *  @return 0 on success, otherwise non-zero;
	 */
	int SessionLogout(void);

	/** Issue Inventory request and return list of objects
	 *
	 *  Throw an exception inheritated from AdminLibException when inventory request is failed
	 *
	 *  @param type object type to make inventory
	 *  @param filter filter string to inventory
	 *  @param force force inventory is required (default = false)
	 *  @return list pointer of LTFS Objects on success, otherwise non-zero;
	 */
	std::list<std::shared_ptr <LTFSObject> > SessionInventory(ltfs_object_t type, std::string filter = "", bool force = false);
	void SessionInventory(std::list<std::shared_ptr<LTFSNode> > &nodes, std::string filter = "", bool force = false);
	void SessionInventory(std::list<std::shared_ptr<Drive> > &drives, std::string filter = "", bool force = false);
	void SessionInventory(std::list<std::shared_ptr<Cartridge> > &cartridges, std::string filter = "", bool force = false);

	/** Issue Action request
	 *
	 *  Throw an exception inheritated from AdminLibException when action request is failed
	 *
	 *  @param msg Action object to be sent to admin channel
	 *  @return pointer of Result Object
	 */
	std::shared_ptr<Result> SessionAction(std::shared_ptr<Action> msg);

	/** Check session is avived or not
	 *
	 *  @return ture if session is alived. Otherwise false
	 */
	bool is_alived() { return ( (connfd_ > 0) ? true : false); }

	std::string get_server() { return server_; }
	int get_port() { return port_; }
	int get_fd() { return connfd_; }

private:
	std::string   server_;
	int           port_;
	std::string   version_;
	int           loglevel_;

	std::mutex  seq_lock_;
	uint64_t      sequence_;

	int           connfd_;
	std::mutex  send_lock_;
	std::mutex  receive_lock_;

	std::mutex  receiver_lock_;
	std::condition_variable receiver_state_;
	std::shared_ptr<std::thread> receiver_;
	std::mutex  response_lock_;
	std::condition_variable response_state_;
	std::unordered_map<uint64_t, std::shared_ptr<LTFSResponseMessage> > received_response_;
	std::string   buf_;
	bool keep_alive_;

	std::shared_ptr<Result> IssueRequest(std::shared_ptr<LTFSRequestMessage> msg, bool terminate_receiver = false);
	int session_send(std::string message);
	std::string session_receive();

	int send_message();
	int receive_message();

	/** Receive a message from LTFS admin channel (thread safety)
	 *
	 *  Receive a message from received_response_. Must call this function when receiver thread is running.
	 */
	std::shared_ptr<LTFSResponseMessage> ReceiveMessage(uint64_t sequence, bool force = false);

	/** Receiver thread function
	 *
	 *  Receive a message from LTFS admin channel and put it into received_response_. Keep runnig while keep_alive_ is true
	 */
	void ReceiveThread(void);

	/** Receive a message from LTFS admin channel raw socket (non thread safety)
     *
	 *  This is low level function to receive a message from LTFS admin channel. Must not call this function when receiver
	 *  thread is running.
	 */
	std::shared_ptr<LTFSResponseMessage> _ReceiveMessage();

	std::shared_ptr<Result> SessionInventory(ltfs_object_t type,
											   std::unordered_map<std::string, std::string> options);

};

}
