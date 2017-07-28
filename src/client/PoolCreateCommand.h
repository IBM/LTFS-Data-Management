#pragma once

class PoolCreateCommand: public OpenLTFSCommand

{
private:
    void talkToBackend(std::stringstream *parmList)
    {
    }
public:
    PoolCreateCommand() :
            OpenLTFSCommand("create", ":+hP:")
    {
    }
    ~PoolCreateCommand()
    {
    }
    void printUsage();
    void doCommand(int argc, char **argv);
};
