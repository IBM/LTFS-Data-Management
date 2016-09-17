#ifndef _INFOFILES_H
#define _INFOFILES_H

class InfoFiles : public Operation

{
private:
public:
    InfoFiles() : Operation("+hf:R:") {};
    ~InfoFiles() {};
    void printUsage();
    void doOperation(int argc, char **argv);
    static constexpr const char *cmd1 = "info";
    static constexpr const char *cmd2 = "files";
};

#endif /* _INFOFILES_H */
