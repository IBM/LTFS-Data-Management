#pragma once

class MigrateCommand: public OpenLTFSCommand

{
private:
    void talkToBackend(std::stringstream *parmList);
public:
    MigrateCommand() :
            OpenLTFSCommand("migrate", ":+hpn:f:P:")
    {
    }
    ~MigrateCommand()
    {
    }
    void printUsage();
    void doCommand(int argc, char **argv);
};
