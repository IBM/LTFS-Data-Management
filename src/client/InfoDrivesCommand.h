#ifndef _INFODRIVES_H
#define _INFODRIVES_H

class InfoDrivesCommand : public OpenLTFSCommand

{
private:
	void talkToBackend(std::stringstream *parmList) {}
public:
    InfoDrivesCommand() : OpenLTFSCommand("drives", ":+h") {};
    ~InfoDrivesCommand() {};
    void printUsage();
    void doCommand(int argc, char **argv);
};

#endif /* _INFODRIVES_H */
