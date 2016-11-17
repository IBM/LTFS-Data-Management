#ifndef _INFOFILES_H
#define _INFOFILES_H

class InfoFilesCommand : public OpenLTFSCommand

{
private:
	void talkToBackend(std::stringstream *parmList);
public:
    InfoFilesCommand() : OpenLTFSCommand("files", ":+hf:") {};
    ~InfoFilesCommand() {};
    void printUsage();
    void doCommand(int argc, char **argv);
};

#endif /* _INFOFILES_H */
