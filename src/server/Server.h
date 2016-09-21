class Server {
private:
	SubServer subServer;
	void lockServer();
public:
	Server() {};
	void initialize();
	void daemonize();
	void run();
};
