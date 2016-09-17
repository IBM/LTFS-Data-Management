#ifndef _INFOFILES_H
#define _INFOFILES_H

class InfoFilesCommand : public OpenLTFSCommand

{
private:
public:
    InfoFilesCommand() : OpenLTFSCommand("+hf:R:") {};
    ~InfoFilesCommand() {};
    void printUsage();
    void doCommand(int argc, char **argv);
    static constexpr const char *command = "files";
};

#endif /* _INFOFILES_H */