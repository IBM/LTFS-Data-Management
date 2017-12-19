#pragma once

class RecallCommand: public LTFSDMCommand

{
private:
    void talkToBackend(std::stringstream *parmList);
public:
    RecallCommand() :
            LTFSDMCommand("recall", ":+hrn:f:")
    {
    }
    ~RecallCommand()
    {
    }
    void printUsage();
    void doCommand(int argc, char **argv);
};
