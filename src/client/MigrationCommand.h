#ifndef _MIGRATION_H
#define _MIGRATION_H

class MigrationCommand : public OpenLTFSCommand

{
private:
	void queryResults();
	void talkToBackend(std::stringstream *parmList);
public:
    MigrationCommand() : OpenLTFSCommand("migrate", ":+hwpc:n:f:R:") {};
    ~MigrationCommand() {};
    void printUsage();
    void doCommand(int argc, char **argv);
};

#endif /* _MIGRATION_H */
