#pragma once

class StartCommand: public LTFSDMCommand

{
private:
    std::stringstream serverPath;
    void determineServerPath();
    void startServer();
    void waitForResponse();
    void talkToBackend(std::stringstream *parmList)
    {
    }
public:
    StartCommand() :
            LTFSDMCommand("start", "")
    {
    }
    ~StartCommand()
    {
    }
    void printUsage();
    void doCommand(int argc, char **argv);
};
