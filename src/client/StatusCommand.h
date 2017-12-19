#pragma once

class StatusCommand: public LTFSDMCommand

{
private:
    std::stringstream serverPath;
    void determineServerPath();
    void talkToBackend(std::stringstream *parmList)
    {
    }
public:
    StatusCommand() :
            LTFSDMCommand("status", "h")
    {
    }
    ~StatusCommand()
    {
    }
    void printUsage();
    void doCommand(int argc, char **argv);
};
