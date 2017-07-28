#pragma once

class PoolAddCommand: public OpenLTFSCommand

{
private:
    void talkToBackend(std::stringstream *parmList)
    {
    }
public:
    PoolAddCommand() :
            OpenLTFSCommand("add", ":+hP:t:")
    {
    }
    ~PoolAddCommand()
    {
    }
    void printUsage();
    void doCommand(int argc, char **argv);
};
