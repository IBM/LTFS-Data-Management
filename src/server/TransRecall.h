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

    void processFiles(int reqNum, std::string tapeId);
public:
    TransRecall()
    {
    }
    ~TransRecall()
    {
    }
    void addRequest(Connector::rec_info_t recinfo, std::string tapeId,
            long reqNum);
    void cleanupEvents();
    void run(Connector *connector);
    static unsigned long recall(Connector::rec_info_t recinfo,
            std::string tapeId, FsObj::file_state state,
            FsObj::file_state toState);

    void execRequest(int reqNum, std::string tapeId);
};
