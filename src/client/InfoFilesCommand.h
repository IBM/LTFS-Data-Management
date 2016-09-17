#ifndef _INFOFILES_H
#define _INFOFILES_H

class InfoFilesCommand : public OpenLTFSCommand

{
private:
public:
    InfoFilesCommand() : OpenLTFSCommand("files", ":+hf:R:") {};
    ~InfoFilesCommand() {};
    void printUsage();
    void doCommand(int argc, char **argv);
};

#endif /* _INFOFILES_H */
