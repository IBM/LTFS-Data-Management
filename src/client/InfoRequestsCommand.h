#pragma once

class InfoRequestsCommand: public LTFSDMCommand

{
private:
    void talkToBackend(std::stringstream *parmList)
    {
    }
public:
    InfoRequestsCommand() :
            LTFSDMCommand("requests", ":+hn:")
    {
    }
    ~InfoRequestsCommand()
    {
    }
    void printUsage();
    void doCommand(int argc, char **argv);
};
