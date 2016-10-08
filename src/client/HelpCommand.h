#ifndef _HELP_H
#define _HELP_H

class HelpCommand : public OpenLTFSCommand

{
private:
	void talkToBackend(std::stringstream *parmList) {}
public:
    HelpCommand() : OpenLTFSCommand("help", "") {};
    ~HelpCommand() {};
    void printUsage();
    void doCommand(int argc, char **argv);
};

#endif /* _HELP_H */
