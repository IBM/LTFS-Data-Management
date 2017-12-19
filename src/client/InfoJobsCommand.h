#pragma once

class InfoJobsCommand: public LTFSDMCommand

{
private:
    void talkToBackend(std::stringstream *parmList)
    {
    }
public:
    InfoJobsCommand() :
            LTFSDMCommand("jobs", ":+hn:")
    {
    }
    ~InfoJobsCommand()
    {
    }
    void printUsage();
    void doCommand(int argc, char **argv);
};
