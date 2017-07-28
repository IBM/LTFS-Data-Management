#pragma once

class TransRecall

{
private:
    static const std::string ADD_JOB;
    static const std::string CHECK_REQUEST_EXISTS;
    static const std::string CHANGE_REQUEST_TO_NEW;
    static const std::string ADD_REQUEST;
    static const std::string REMAINING_JOBS;
    static const std::string SET_RECALLING;
    static const std::string SELECT_JOBS;
    static const std::string DELETE_JOBS;
    static const std::string COUNT_REMAINING_JOBS;
    static const std::string DELETE_REQUEST;

public:
    TransRecall()
    {
    }
    ~TransRecall()
    {
    }
    static void addRequest(Connector::rec_info_t recinfo, std::string tapeId,
            long reqNum);
    void cleanupEvents();
    void run(Connector *connector);
    static unsigned long recall(Connector::rec_info_t recinfo,
            std::string tapeId, FsObj::file_state state,
            FsObj::file_state toState);

    static void recallStep(int reqNum, std::string tapeId);
    static void execRequest(int reqNum, std::string tapeId);
};
