#pragma once

class SelRecall: public FileOperation
{
private:
    unsigned long pid;
    long reqNumber;
    std::set<std::string> needsTape;
    int targetState;
    static unsigned long recall(std::string fileName, std::string tapeId,
            FsObj::file_state state, FsObj::file_state toState);
    bool processFiles(std::string tapeId, FsObj::file_state toState,
            bool needsTape);

    static const std::string ADD_JOB;
    static const std::string GET_TAPES;
    static const std::string ADD_REQUEST;
    static const std::string SET_RECALLING;
    static const std::string SELECT_JOBS;
    static const std::string FAIL_JOB;
    static const std::string SET_JOB_SUCCESS;
    static const std::string RESET_JOB_STATE;
    static const std::string UPDATE_REQUEST;
public:
    SelRecall(unsigned long _pid, long _reqNumber, int _targetState) :
            pid(_pid), reqNumber(_reqNumber), targetState(_targetState)
    {
    }
    void addJob(std::string fileName);
    void addRequest();
    void execRequest(std::string tapeId, bool needsTape);
};
