#ifndef _COMM_H
#define _COMM_H

class LTFSDmComm : public LTFSDmProtocol::Command {
public:
	LTFSDmComm() {}
	~LTFSDmComm() {}
	void send(int fd);
	void recv(int fd);
};

class LTFSDmCommClient : public LTFSDmComm {
private:
	std::atomic<int> socRefFd;
public:
	LTFSDmCommClient() : socRefFd(Const::UNSET) {}
	~LTFSDmCommClient() { if ( socRefFd != Const::UNSET ) close(socRefFd); }
	void connect();
	void send() { return LTFSDmComm::send(socRefFd); }
	void recv() { return LTFSDmComm::recv(socRefFd); }
};

class LTFSDmCommServer : public LTFSDmComm {
private:
	std::atomic<int> socRefFd;
	std::atomic<int> socAccFd;
public:
	LTFSDmCommServer() : socRefFd(Const::UNSET), socAccFd(Const::UNSET) {}
	~LTFSDmCommServer() { if ( socAccFd != Const::UNSET ) { ::close(socAccFd); socAccFd = Const::UNSET; }; if ( socRefFd != Const::UNSET ) { ::close(socRefFd); socRefFd = Const::UNSET;} }
	void listen();
	void accept();
	void close() {::close(socAccFd); socAccFd = Const::UNSET;}
	void send() { return LTFSDmComm::send(socAccFd); }
	void recv() { return LTFSDmComm::recv(socAccFd); }
};


#endif /* _COMM_H */
