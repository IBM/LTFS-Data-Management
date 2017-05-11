namespace ltfsadmin {

class Logout:public LTFSRequestMessage {
public:
	Logout() :
		LTFSRequestMessage(LTFS_MSG_LOGOUT, 0) {};
	Logout(uint64_t sequence) :
		LTFSRequestMessage(LTFS_MSG_LOGOUT, sequence) {};
	virtual ~Logout() {};
};

}
