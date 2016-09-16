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
};

#endif /* _MIGRATION_H */
