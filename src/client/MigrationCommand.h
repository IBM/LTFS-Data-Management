#ifndef _MIGRATION_H
#define _MIGRATION_H

class MigrationCommand : public OpenLTFSCommand

{
private:
	void talkToBackend(std::stringstream *parmList);
public:
    MigrationCommand() : OpenLTFSCommand("migrate", ":+hwpn:f:P:") {};
    ~MigrationCommand() {};
    void printUsage();
    void doCommand(int argc, char **argv);
};

#endif /* _MIGRATION_H */
