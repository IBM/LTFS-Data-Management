#pragma once

class PoolDeleteCommand: public OpenLTFSCommand

{
private:
    void talkToBackend(std::stringstream *parmList)
    {
    }
public:
    PoolDeleteCommand() :
            OpenLTFSCommand("delete", ":+hP:")
    {
    }
    ~PoolDeleteCommand()
    {
    }
    void printUsage();
    void doCommand(int argc, char **argv);
};
