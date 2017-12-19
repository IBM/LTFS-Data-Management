#pragma once

class PoolDeleteCommand: public LTFSDMCommand

{
private:
    void talkToBackend(std::stringstream *parmList)
    {
    }
public:
    PoolDeleteCommand() :
            LTFSDMCommand("delete", ":+hP:")
    {
    }
    ~PoolDeleteCommand()
    {
    }
    void printUsage();
    void doCommand(int argc, char **argv);
};
