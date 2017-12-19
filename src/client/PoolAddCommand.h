#pragma once

class PoolAddCommand: public LTFSDMCommand

{
private:
    void talkToBackend(std::stringstream *parmList)
    {
    }
public:
    PoolAddCommand() :
            LTFSDMCommand("add", ":+hP:t:")
    {
    }
    ~PoolAddCommand()
    {
    }
    void printUsage();
    void doCommand(int argc, char **argv);
};
