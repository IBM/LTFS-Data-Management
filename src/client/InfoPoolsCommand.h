#ifndef _INFOPOOLS_H
#define _INFOPOOLS_H

class InfoPoolsCommand : public OpenLTFSCommand

{
private:
	void talkToBackend(std::stringstream *parmList) {}
public:
    InfoPoolsCommand() : OpenLTFSCommand("pools", ":+h") {};
    ~InfoPoolsCommand() {};
    void printUsage();
    void doCommand(int argc, char **argv);
};

#endif /* _INFOPOOLS_H */
