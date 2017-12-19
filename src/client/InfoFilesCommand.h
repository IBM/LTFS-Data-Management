#pragma once

class InfoFilesCommand: public LTFSDMCommand

{
private:
    void talkToBackend(std::stringstream *parmList);
public:
    InfoFilesCommand() :
            LTFSDMCommand("files", ":+hf:")
    {
    }
    ~InfoFilesCommand()
    {
    }
    void printUsage();
    void doCommand(int argc, char **argv);
};
