#pragma once

class InfoCommand: public LTFSDMCommand

{
private:
    void talkToBackend(std::stringstream *parmList)
    {
    }
public:
    InfoCommand() :
            LTFSDMCommand("info", "")
    {
    }
    void printUsage()
    {
        INFO(LTFSDMC0020I);
    }
    ;
    void doCommand(int argc, char **argv)
    {
    }
};
