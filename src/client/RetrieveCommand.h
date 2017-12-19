#pragma once

class RetrieveCommand: public LTFSDMCommand

{
private:
    std::stringstream serverPath;
    void determineServerPath();
    void talkToBackend(std::stringstream *parmList)
    {
    }
public:
    RetrieveCommand() :
            LTFSDMCommand("retrieve", "h")
    {
    }
    ~RetrieveCommand()
    {
    }
    void printUsage();
    void doCommand(int argc, char **argv);
};
