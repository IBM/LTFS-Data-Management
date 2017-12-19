#pragma once

class StopCommand: public LTFSDMCommand

{
private:
    void talkToBackend(std::stringstream *parmList)
    {
    }
public:
    StopCommand() :
            LTFSDMCommand("stop", "hx")
    {
    }
    ~StopCommand()
    {
    }
    void printUsage();
    void doCommand(int argc, char **argv);
};
