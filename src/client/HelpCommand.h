#pragma once

class HelpCommand: public LTFSDMCommand

{
private:
    void talkToBackend(std::stringstream *parmList)
    {
    }
public:
    HelpCommand() :
            LTFSDMCommand("help", "")
    {
    }
    ~HelpCommand()
    {
    }
    void printUsage();
    void doCommand(int argc, char **argv);
};
