extern std::atomic<bool> terminate;

class Server {
private:
	SubServer subServer;
	Connector connector;
	long key;
	void lockServer();
	void writeKey();
public:
	Server() {};
	void initialize();
	void daemonize();
	void run();
};
