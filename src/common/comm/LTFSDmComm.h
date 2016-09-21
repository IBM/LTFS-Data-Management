#ifndef _COMM_H
#define _COMM_H

class LTFSDmComm : public LTFSDmProtocol::Command {
public:
	LTFSDmComm() {}
	~LTFSDmComm() {}
	virtual void connect() {}
	void send(int fd);
	void recv(int fd);
};

class LTFSDmCommClient : public LTFSDmComm {
private:
	int socRefFd;
public:
	LTFSDmCommClient() : socRefFd(-1) {}
	~LTFSDmCommClient() { if ( socRefFd != -1 ) close(socRefFd); }
	void connect();
	void send() { return LTFSDmComm::send(socRefFd); }
	void recv() { return LTFSDmComm::recv(socRefFd); }
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
	void send() { return LTFSDmComm::send(socAccFd); }
	void recv() { return LTFSDmComm::recv(socAccFd); }
};


#endif /* _COMM_H */
