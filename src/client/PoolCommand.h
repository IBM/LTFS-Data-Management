#pragma once

class PoolCommand: public LTFSDMCommand

{
private:
    void talkToBackend(std::stringstream *parmList)
    {
    }
public:
    PoolCommand() :
            LTFSDMCommand("pool", "")
    {
    }
    void printUsage()
    {
        INFO(LTFSDMC0073I);
    }
    ;
    void doCommand(int argc, char **argv)
    {
    }
};
