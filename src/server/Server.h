class Server {
private:
	SubServer subServer;
	void lockServer();
	void writeKey();
public:
	Server() {};
	void initialize();
	void daemonize();
	void run();
};
