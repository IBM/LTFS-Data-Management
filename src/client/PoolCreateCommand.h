#pragma once

class PoolCreateCommand: public LTFSDMCommand

{
private:
    void talkToBackend(std::stringstream *parmList)
    {
    }
public:
    PoolCreateCommand() :
            LTFSDMCommand("create", ":+hP:")
    {
    }
    ~PoolCreateCommand()
    {
    }
    void printUsage();
    void doCommand(int argc, char **argv);
};
