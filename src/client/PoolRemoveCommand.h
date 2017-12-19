#pragma once

class PoolRemoveCommand: public LTFSDMCommand

{
private:
    void talkToBackend(std::stringstream *parmList)
    {
    }
public:
    PoolRemoveCommand() :
            LTFSDMCommand("remove", ":+hP:t:")
    {
    }
    ~PoolRemoveCommand()
    {
    }
    void printUsage();
    void doCommand(int argc, char **argv);
};
