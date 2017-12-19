#pragma once

class InfoTapesCommand: public LTFSDMCommand

{
private:
    void talkToBackend(std::stringstream *parmList)
    {
    }
public:
    InfoTapesCommand() :
            LTFSDMCommand("tapes", ":+h")
    {
    }
    ~InfoTapesCommand()
    {
    }
    void printUsage();
    void doCommand(int argc, char **argv);
};
