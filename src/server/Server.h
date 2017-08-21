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

    static ThreadPool<Migration::mig_info_t,
            std::shared_ptr<std::list<unsigned long>>> *wqs;

    static std::string getTapeName(FsObj *diskfile, std::string tapeId);
    static std::string getTapeName(unsigned long long fsid, unsigned int igen,
            unsigned long long ino, std::string tapeId);
    static long getStartBlock(std::string tapeName);
    static void createDir(std::string path);
    static void createLink(std::string tapeId, std::string origPath,
            std::string dataPath);
    static void createDataDir(std::string tapeId);

    Server() :
            key(Const::UNSET)
    {
    }
    void initialize(bool dbUseMemory);
    void daemonize();
    void run(Connector *connector, sigset_t set);
};
