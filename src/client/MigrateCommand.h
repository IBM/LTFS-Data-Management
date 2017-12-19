#pragma once

class MigrateCommand: public LTFSDMCommand

{
private:
    void talkToBackend(std::stringstream *parmList);
public:
    MigrateCommand() :
            LTFSDMCommand("migrate", ":+hpn:f:P:")
    {
    }
    ~MigrateCommand()
    {
    }
    void printUsage();
    void doCommand(int argc, char **argv);
};
