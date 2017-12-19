#pragma once

class AddCommand: public LTFSDMCommand

{
private:
    void talkToBackend(std::stringstream *parmList)
    {
    }
public:
    AddCommand() :
            LTFSDMCommand("add", ":+")
    {
    }
    ~AddCommand()
    {
    }
    void printUsage();
    void doCommand(int argc, char **argv);
};
