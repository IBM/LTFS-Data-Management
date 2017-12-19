#pragma once

class InfoFsCommand: public LTFSDMCommand

{
private:
    void talkToBackend(std::stringstream *parmList);
public:
    InfoFsCommand() :
            LTFSDMCommand("fs", "h")
    {
    }
    ~InfoFsCommand()
    {
    }
    void printUsage();
    void doCommand(int argc, char **argv);
};
