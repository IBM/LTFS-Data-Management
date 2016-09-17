#ifndef _MIGRATION_H
#define _MIGRATION_H

class MigrationCommand : public OpenLTFSCommand

{
private:
public:
    MigrationCommand() : OpenLTFSCommand("+hwpc:n:f:R:") {};
    ~MigrationCommand() {};
    void printUsage();
    void doCommand(int argc, char **argv);
    static constexpr const char *command = "migrate";
};

#endif /* _MIGRATION_H */
