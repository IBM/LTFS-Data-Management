#ifndef _MIGRATION_H
#define _MIGRATION_H

class Migration : public Operation

{
private:
public:
    Migration() : Operation("+hwpc:n:f:R:") {};
    ~Migration() {};
    void printUsage();
    void doOperation(int argc, char **argv);
    static constexpr const char *cmd1 = "migrate";
    static constexpr const char *cmd2 = "";
};

#endif /* _MIGRATION_H */
