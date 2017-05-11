#include <iostream>
#include <sstream>

#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <sys/ioctl.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>

#include <memory>
#include <list>
#include <condition_variable>
#include <thread>


#include "LTFSAdminSession.h"
#include "Identifier.h"
#include "Result.h"
#include "Login.h"
#include "Logout.h"
#include "Inventory.h"
#include "Action.h"
#include "SessionError.h"
#include "RequestError.h"

using namespace ltfsadmin;

LTFSAdminSession::LTFSAdminSession(std::string server, int port)
	: server_(server), buf_("")
{
	port_       = port;

	sequence_   = 0;
	connfd_     = -1;
	received_response_.clear();

	buf_        = "";
	keep_alive_ = false;

	return;
}

LTFSAdminSession::~LTFSAdminSession()
{
	if (keep_alive_)
		Disconnect();

	Log(DEBUG2, "Destruct LTFSAdminSession");
}

int LTFSAdminSession::Connect()
{
	struct addrinfo *servaddr_info = NULL, *addr_info = NULL;
	struct addrinfo hint_info;

	std::ostringstream msg_stream;
	std::ostringstream convert;
	convert << port_;
	std::string port_str = convert.str();

	Log(DEBUG2, "Connecting");

	memset(&hint_info, 0, sizeof(hint_info));
	hint_info.ai_family   = AF_UNSPEC;
	hint_info.ai_socktype = SOCK_STREAM;
	hint_info.ai_flags    = AI_NUMERICSERV;

	int ret = getaddrinfo(server_.c_str(), port_str.c_str(), &hint_info, &servaddr_info);
	if (ret) {
		std::vector<std::string> args;
		args.push_back("Cannot get address info");
		args.push_back(server_);
		throw SessionError(__FILE__, __LINE__, "001E", args);
		return ret;
	}

	for (addr_info = servaddr_info; addr_info != NULL; addr_info = addr_info->ai_next) {
		connfd_ = socket(addr_info->ai_family, addr_info->ai_socktype, addr_info->ai_protocol);
		if (connfd_ < 0)
			continue;

		ret = connect(connfd_, addr_info->ai_addr, addr_info->ai_addrlen);
		if (ret < 0) {
			close(connfd_);
			connfd_ = -1;
			continue;
		}

		break;
	}

	if (fcntl(connfd_, F_GETFL) & O_NONBLOCK) {
		Log(DEBUG2, "Detecting teh connection is NON Blocking mode. Correct it.");
		ioctl(connfd_, FIONBIO, 0);
	}

	if (servaddr_info)
		freeaddrinfo(servaddr_info);

	if (connfd_ < 0) {
		std::vector<std::string> args;
		args.push_back("Cannot connect to server");
		args.push_back(server_);
		throw SessionError(__FILE__, __LINE__, "002E", args);
	}

	return connfd_;
}

int LTFSAdminSession::Disconnect()
{
	Log(DEBUG2, "Disconnecting");

	if (receiver_) {
		Log(DEBUG2, "Waiting to exit receiver thread");
		receiver_->join();
		receiver_ = std::shared_ptr<std::thread>();;
	}

	if (connfd_ > 0) {
		Log(DEBUG2, "Closing Socket");
		close(connfd_);
		connfd_ = -1;
		sequence_ = 0;
	}

	Log(DEBUG2, "Disconnected");

	return 0;
}

uint64_t LTFSAdminSession::GetSequence(void)
{
	uint64_t result;

	seq_lock_.lock();
	result = ++sequence_;
	seq_lock_.unlock();

	return result;
}

std::shared_ptr<Result> LTFSAdminSession::IssueRequest(std::shared_ptr<LTFSRequestMessage> msg, bool terminate_receiver)
{
	session_send(msg->ToString());

	if (terminate_receiver)
		keep_alive_ = false;

	std::shared_ptr<Result> res =
		std::dynamic_pointer_cast<Result>(ReceiveMessage(msg->GetSequence(), terminate_receiver));

	return res;
}

int LTFSAdminSession::SessionLogin(std::string user, std::string password)
{
	int rc = 0;

	std::shared_ptr<Identifier> obj =
		std::dynamic_pointer_cast<Identifier>(_ReceiveMessage());
	std::ostringstream msg_stream;

	if (obj && obj->GetType() != LTFS_MSG_IDENTIFIER){
		std::vector<std::string> args;
		args.push_back("Response is not Identifier message");
		throw RequestError(__FILE__, __LINE__, "003E", args);
		return rc;
	}

	// Kick background message receiver
	std::unique_lock<std::mutex> receiver_lock(receiver_lock_);
	receiver_ = std::shared_ptr<std::thread>(new std::thread( &LTFSAdminSession::ReceiveThread, this));
	Log(DEBUG2, "Sleep until receiver thread is started.");
	receiver_state_.wait(receiver_lock);
	Log(DEBUG2, "Wake up because receiver thread is running");
	receiver_lock.unlock();

	// Issue Login request to LTFS
	std::shared_ptr<Login> l;
	if (obj->GetAuth())
		l = std::shared_ptr<Login>(new Login(GetSequence(), user, password));
	else
		l = std::shared_ptr<Login>(new Login(GetSequence()));

	std::shared_ptr<Result> msg_result =
		std::dynamic_pointer_cast<Result>(IssueRequest(l));
	if (msg_result && msg_result->GetStatus() != LTFS_ADMIN_SUCCESS) {
		rc = -1;
		std::vector<std::string> args;
		args.push_back("Login request is failed");
		throw RequestError(__FILE__, __LINE__, "004E", args);
	}

	return rc;
}

int LTFSAdminSession::SessionLogout(void)
{
	int rc = 0;
	std::ostringstream msg_stream;
	std::shared_ptr<Logout> l = std::shared_ptr<Logout>(new Logout(GetSequence()));

	std::shared_ptr<Result> msg_result =
		std::dynamic_pointer_cast<Result>(IssueRequest(l, true));
	if (msg_result && msg_result->GetStatus() != LTFS_ADMIN_SUCCESS) {
		rc = -1;
		std::vector<std::string> args;
		args.push_back("Logout request is failed");
		throw RequestError(__FILE__, __LINE__, "005E", args);
	}

	return rc;
}

std::shared_ptr<Result> LTFSAdminSession::SessionInventory(ltfs_object_t type, std::unordered_map<std::string, std::string> options)
{
	std::string str_type;
	std::ostringstream msg_stream;
	std::shared_ptr<Result> null_pointer = std::shared_ptr<Result>();

	try {
		std::vector<std::string> args;
		switch (type) {
			case LTFS_OBJ_LTFS_NODE:
				str_type = "LTFSNode";
				break;
			case LTFS_OBJ_LIBRARY:
				str_type = "library";
				break;
			case LTFS_OBJ_DRIVE:
				str_type = "drive";
				break;
			case LTFS_OBJ_CARTRIDGE:
				str_type = "cartridge";
				break;
			case LTFS_OBJ_HISTORY:
				str_type = "history";
				args.push_back("Unsupported object is detected");
				args.push_back(str_type);
				throw RequestError(__FILE__, __LINE__, "006E", args);
				break;
			case LTFS_OBJ_JOB:
				str_type = "job";
				args.push_back("Unsupported object is detected");
				args.push_back(str_type);
				throw RequestError(__FILE__, __LINE__, "006E", args);
				break;
			case LTFS_OBJ_PRIVILEGE:
				str_type = "privilege";
				args.push_back("Unsupported object is detected");
				args.push_back(str_type);
				throw RequestError(__FILE__, __LINE__, "006E", args);
				break;
			default:
				args.push_back("Unsupported object is detected");
				args.push_back("Unknown");
				throw RequestError(__FILE__, __LINE__, "006E", args);
				break;
		}
	} catch(RequestError e) {
		throw e;
		return null_pointer;
	}

	std::shared_ptr<Inventory> i = std::shared_ptr<Inventory>(new Inventory(GetSequence(), str_type, options));

	std::shared_ptr<Result> msg_result = IssueRequest(i);
	if (msg_result && msg_result->GetStatus() != LTFS_ADMIN_SUCCESS) {
		std::list<std::shared_ptr<LTFSObject> > empty_list;
		std::vector<std::string> args;
		args.push_back("Inventory request is failed");
		args.push_back(str_type);
		throw RequestError(__FILE__, __LINE__, "007E", args);
		return null_pointer;
	}

	return msg_result;
}

std::list<std::shared_ptr <LTFSObject> > LTFSAdminSession::SessionInventory(ltfs_object_t type, std::string filter, bool force)
{
	std::unordered_map<std::string, std::string> options;

	if (filter.length())
		options["filter"] = filter;

	if (force)
		options["force"] = "true";

	std::shared_ptr<Result> result = SessionInventory(type, options);
	std::list<std::shared_ptr<LTFSObject> > obj_list;
	if (result)
		obj_list = result->GetObjects();

	return obj_list;
}

void LTFSAdminSession::SessionInventory(std::list<std::shared_ptr<LTFSNode> > &nodes, std::string filter, bool force)
{
	std::unordered_map<std::string, std::string> options;

	if (filter.length())
		options["filter"] = filter;

	if (force)
		options["force"] = "true";

	std::shared_ptr<Result> result = SessionInventory(LTFS_OBJ_LTFS_NODE, options);
	if (result)
		nodes = result->GetLTFSNodes();

	return;
}

void LTFSAdminSession::SessionInventory(std::list<std::shared_ptr<Cartridge> > &cartridges, std::string filter, bool force)
{
	std::unordered_map<std::string, std::string> options;

	if (filter.length())
		options["filter"] = filter;

	if (force)
		options["force"] = "true";

	std::shared_ptr<Result> result = SessionInventory(LTFS_OBJ_CARTRIDGE, options);
	if (result)
		cartridges = result->GetCartridges();

	return;
}

void LTFSAdminSession::SessionInventory(std::list<std::shared_ptr<Drive> > &drives, std::string filter, bool force)
{
	std::unordered_map<std::string, std::string> options;

	if (filter.length())
		options["filter"] = filter;

	if (force)
		options["force"] = "true";

	std::shared_ptr<Result> result = SessionInventory(LTFS_OBJ_DRIVE, options);
	if (result)
		drives = result->GetDrives();

	return;
}

std::shared_ptr<Result> LTFSAdminSession::SessionAction(std::shared_ptr<Action> msg)
{
	session_send(msg->ToString());
	std::shared_ptr<Result> res =
		std::dynamic_pointer_cast<Result>(ReceiveMessage(msg->GetSequence()));

	return res;
}

int LTFSAdminSession::session_send(std::string message)
{
	ssize_t len, n_written;

	send_lock_.lock();
	len = message.length() + 1;
	n_written = send(connfd_, message.c_str(), len, 0);
	send_lock_.unlock();

	if (n_written < 0) {
		std::vector<std::string> args;
		args.push_back("Cannot send message");
		args.push_back(server_);
		throw SessionError(__FILE__, __LINE__, "008E", args);
	}

	Log(DEBUG2, "Message Sent");
	Log(DEBUG3, "Message Sent:\n" + message);

	return n_written;
}

std::string LTFSAdminSession::session_receive()
{
	ssize_t n_read = 0;
	std::string::size_type found;
	char buf[2048];
	std::string msg;

	/* Receive new data from the socket if current buffer has no delimiter */
	if ((found = buf_.find_first_of('\0', 0)) == std::string::npos) {
		memset(buf, 0, sizeof(buf));
		while (true) {
			n_read = recv(connfd_, buf, sizeof(buf) - 1, 0);

			std::ostringstream os;
			os << "Receive from socket: " << n_read;
			Log(DEBUG3, os.str().c_str());

			if (n_read < 0 && errno == EINTR) {
				Msg(INFO, "101I");
			} else if (n_read < 0) {
				Msg(INFO, "102I", connfd_, errno);
				close(connfd_);
				connfd_ = -1;
				sequence_ = 0;
				keep_alive_ = false;
				msg = std::string(buf_);
				buf_.clear();
				break;
			} else if (n_read == 0) {
				Msg(INFO, "100I", connfd_);
				close(connfd_);
				connfd_ = -1;
				sequence_ = 0;
				keep_alive_ = false;
				msg = std::string(buf_);
				buf_.clear();
				break;
			} else {
				buf_.append(buf, n_read);
				if ((found = buf_.find_first_of('\0', 0)) != std::string::npos)
					break;
				memset(buf, 0, n_read);
			}
		}
	}

	if (found != std::string::npos) {
		msg = std::string(buf_, 0, found);
		if (found == buf_.length() - 1) {
			buf_.clear();
		} else
			buf_ = std::string(buf_, found + 1);
			std::ostringstream os;
			os << "Receive message: " << found;
			Log(DEBUG0, os.str().c_str());
	}

	Log(DEBUG2, "Message Received");
	Log(DEBUG3, "Message:\n" + msg);

	return msg;
}

std::shared_ptr<LTFSResponseMessage> LTFSAdminSession::_ReceiveMessage()
{
	std::shared_ptr<LTFSResponseMessage> obj;

	receive_lock_.lock();
	std::string xml = session_receive();
	std::shared_ptr<LTFSResponseMessage> res = std::shared_ptr<LTFSResponseMessage>(new LTFSResponseMessage(xml));

	switch(res->GetType()) {
		case LTFS_MSG_IDENTIFIER:
			obj = std::shared_ptr<LTFSResponseMessage>(new Identifier(xml));
			break;
		case LTFS_MSG_RESULT:
			obj = std::shared_ptr<LTFSResponseMessage>(new Result(xml, this));
			break;
		default:
			std::vector<std::string> args;
			args.push_back("Unexpected message type is detected");
			args.push_back(server_);
			receive_lock_.unlock();
			throw SessionError(__FILE__, __LINE__, "009E", args);
			break;
	}

	receive_lock_.unlock();

	return obj;
}

void LTFSAdminSession::ReceiveThread(void)
{
	std::shared_ptr<LTFSResponseMessage> obj;

	std::unique_lock<std::mutex> receiver_lock(receiver_lock_);
	keep_alive_ = true;
	Log(DEBUG2, "Notify receiver thread is running");
	receiver_state_.notify_one();
	receiver_lock.unlock();

	while (keep_alive_) {
		receive_lock_.lock();
		std::string xml = session_receive();
		if (xml.size()) {
			std::shared_ptr<LTFSResponseMessage> res = std::shared_ptr<LTFSResponseMessage>(new LTFSResponseMessage(xml));

			switch(res->GetType()) {
				case LTFS_MSG_IDENTIFIER:
					obj = std::shared_ptr<LTFSResponseMessage>(new Identifier(xml));
					break;
				case LTFS_MSG_RESULT:
					obj = std::shared_ptr<LTFSResponseMessage>(new Result(xml, this));
					break;
				default:
					std::vector<std::string> args;
					args.push_back("Unexpected message type is detected");
					args.push_back(server_);
					receive_lock_.unlock();
					throw SessionError(__FILE__, __LINE__, "010E", args);
					break;
			}

			std::unique_lock<std::mutex> response_lock(response_lock_);
			received_response_[res->GetSequence()] = obj;
			response_state_.notify_all();
			response_lock.unlock();
		} else {
			std::unique_lock<std::mutex> response_lock(response_lock_);
			response_state_.notify_all();
			response_lock.unlock();
		}

		receive_lock_.unlock();
	}

	Log(DEBUG0, "Terminating Receiver Thread");

	return;
}

static const char *session_error_xml =
"<?xml version=\"1.0\" encoding=\"UTF-8\"?><ltfsadmin_message version=\"2.0.0\" sequence=\"0\" type=\"result\"><status>Disconnected</status></ltfsadmin_message>";

std::shared_ptr<LTFSResponseMessage> LTFSAdminSession::ReceiveMessage(uint64_t sequence, bool force)
{
	std::shared_ptr<LTFSResponseMessage> obj;
	int count = 0;

	while (keep_alive_ || force) {
		std::unique_lock<std::mutex> response_lock(response_lock_);

		if (received_response_.find(sequence) != received_response_.end()) {
			obj = received_response_[sequence];
			received_response_.erase(sequence);
			break;
		}

		response_state_.wait_for(response_lock, std::chrono::seconds(10));
		Log(DEBUG2, "Wake up because of Message Receive Event");

		/* Check message one more time on Logout case */
		if (force && count)
			break;

		count++;
	}

	if (!keep_alive_ && !obj) {
		Log(DEBUG0, "NULL object is returning because of session error...");
		std::string xml(session_error_xml);
		obj = std::shared_ptr<LTFSResponseMessage>(new Result(xml, this));
	}

	return obj;
}
