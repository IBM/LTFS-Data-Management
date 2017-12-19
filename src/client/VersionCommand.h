#pragma once

class VersionCommand: public LTFSDMCommand

{
private:
    void talkToBackend(std::stringstream *parmList);
public:
    VersionCommand() :
            LTFSDMCommand("version", "h")
    {
    }
    ~VersionCommand()
    {
    }
    void printUsage();
    void doCommand(int argc, char **argv);
};
