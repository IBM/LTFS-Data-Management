#pragma once

class OpenLTFSCommand
{
protected:
    OpenLTFSCommand(std::string command_, std::string optionStr_) :
            preMigrate(false), recToResident(false), requestNumber(
                    Const::UNSET), fileList(""), command(command_), optionStr(
                    optionStr_), fsName(""), mountPoint(""), startTime(
                    time(NULL)), poolNames(""), tapeList( { }), forced(false), key(
                    Const::UNSET), commCommand(Const::CLIENT_SOCKET_FILE)
    {
    }
    bool preMigrate;
    bool recToResident;
    long requestNumber;
    std::string fileList;
    std::string command;
    std::string optionStr;
    std::string fsName;
    std::string mountPoint;
    std::ifstream fileListStrm;
    time_t startTime;
    std::string poolNames;
    std::list<std::string> tapeList;
    bool forced;
    long key;
    LTFSDmCommClient commCommand;

    void getRequestNumber();
    void queryResults();

    void checkOptions(int argc, char **argv);
    virtual void talkToBackend(std::stringstream *parmList)
    {
    }

public:
    virtual ~OpenLTFSCommand();
    virtual void printUsage() = 0;
    virtual void doCommand(int argc, char **argv) = 0;

    // non-virtual methods
    void processOptions(int argc, char **argv);
    void traceParms();
    bool compare(std::string name)
    {
        return !command.compare(name);
    }
    void connect();
    void sendObjects(std::stringstream *parmList);
    void isValidRegularFile();
};
