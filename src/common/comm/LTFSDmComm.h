#ifndef _COMM_H
#define _COMM_H

class LTFSDmComm : public LTFSDmProtocol::Command {
public:
	LTFSDmComm() {}
	~LTFSDmComm() {}
	virtual void connect() {}
	int send(int fd);
	int recv(int fd);
};

class LTFSDmCommClient : public LTFSDmComm {
private:
	int socRefFd;
public:
	LTFSDmCommClient() : socRefFd(-1) {}
	~LTFSDmCommClient() { if ( socRefFd != -1 ) close(socRefFd); }
	void connect();
	int send() { return LTFSDmComm::send(socRefFd); }
	int recv() { return LTFSDmComm::recv(socRefFd); }
};

class LTFSDmCommServer : public LTFSDmComm {
private:
	int socRefFd;
	int socAccFd;
public:
	LTFSDmCommServer() : socRefFd(-1), socAccFd(-1) {}
	~LTFSDmCommServer() { if ( socAccFd != -1 ) close(socAccFd); if ( socRefFd != -1 ) close(socRefFd); }
	void connect();
	void accept();
	int send() { return LTFSDmComm::send(socAccFd); }
	int recv() { return LTFSDmComm::recv(socAccFd); }
};


#endif /* _COMM_H */
