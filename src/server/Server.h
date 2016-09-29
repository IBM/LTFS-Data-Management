extern std::atomic<bool> terminate;

class Server {
private:
	SubServer subServer;
	long key;
	void lockServer();
	void writeKey();
public:
	Server() {};
	void initialize();
	void daemonize();
	void run();
};
