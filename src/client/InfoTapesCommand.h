#ifndef _INFOTAPES_H
#define _INFOTAPES_H

class InfoTapesCommand : public OpenLTFSCommand

{
private:
	void talkToBackend(std::stringstream *parmList) {}
public:
    InfoTapesCommand() : OpenLTFSCommand("tapes", ":+h") {};
    ~InfoTapesCommand() {};
    void printUsage();
    void doCommand(int argc, char **argv);
};

#endif /* _INFOTAPES_H */
