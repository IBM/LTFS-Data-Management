#pragma once

class InfoDrivesCommand: public LTFSDMCommand

{
private:
    void talkToBackend(std::stringstream *parmList)
    {
    }
public:
    InfoDrivesCommand() :
            LTFSDMCommand("drives", ":+h")
    {
    }
    ~InfoDrivesCommand()
    {
    }
    void printUsage();
    void doCommand(int argc, char **argv);
};
