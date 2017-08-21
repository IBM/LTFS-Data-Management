#pragma once

class Scheduler

{
private:
    DataBase::operation op;
    int reqNum;
    int numRepl;
    int replNum;
    LTFSDmProtocol::LTFSDmMigRequest::State tgtState;
    std::string tapeId;
    std::string pool;
    SubServer subs;

    bool poolResAvail(unsigned long minFileSize);
    bool tapeResAvail();
    bool resAvail(unsigned long minFileSize);
    unsigned long smallestMigJob(int reqNum, int replNum);

    static const std::string SELECT_REQUEST;
    static const std::string UPDATE_MIG_REQUEST;
    static const std::string UPDATE_REC_REQUEST;
    static const std::string SMALLEST_MIG_JOB;
public:
    static std::mutex mtx;
    static std::condition_variable cond;
    static std::mutex updmtx;
    static std::condition_variable updcond;
    static std::map<int, std::atomic<bool>> updReq;
    static std::map<std::string, std::atomic<bool>> suspend_map;

    static void mount(std::string driveid, std::string cartridgeid)
    {
        inventory->mount(driveid, cartridgeid);
    }
    static void unmount(std::string driveid, std::string cartridgeid)
    {
        inventory->unmount(driveid, cartridgeid);
    }
    Scheduler() :
            op(DataBase::NOOP), reqNum(Const::UNSET), numRepl(Const::UNSET), replNum(
                    Const::UNSET), tgtState(
                    LTFSDmProtocol::LTFSDmMigRequest::MIGRATED)
    {
    }
    ~Scheduler()
    {
    }
    void run(long key);
};
