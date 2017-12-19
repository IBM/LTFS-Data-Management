#pragma once

class InfoPoolsCommand: public LTFSDMCommand

{
private:
    void talkToBackend(std::stringstream *parmList)
    {
    }
public:
    InfoPoolsCommand() :
            LTFSDMCommand("pools", ":+h")
    {
    }
    ~InfoPoolsCommand()
    {
    }
    void printUsage();
    void doCommand(int argc, char **argv);
};
