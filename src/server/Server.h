class Server {
private:
	void lockServer();
public:
	Server() {};
	void initialize();
	void daemonize();
};
