class Server
{
private:
    SubServer subServer;
    long key;
    void lockServer();
    void writeKey();
    static void signalHandler(sigset_t set, long key);
public:
    static std::mutex termmtx;
    static std::condition_variable termcond;
    static std::atomic<bool> terminate;
    static std::atomic<bool> forcedTerminate;
    static std::atomic<bool> finishTerminate;

    Server() :
            key(Const::UNSET)
    {
    }
    void initialize(bool dbUseMemory);
    void daemonize();
    void run(Connector *connector, sigset_t set);
};
