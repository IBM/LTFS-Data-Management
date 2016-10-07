#ifndef _STATUS_H
#define _STATUS_H

class StatusCommand : public OpenLTFSCommand

{
private:
	std::stringstream serverPath;
	void determineServerPath();
	void checkOptions(int argc, char **argv) {}
	void talkToBackend(std::stringstream *parmList) {}
public:
    StatusCommand() : OpenLTFSCommand("status", "") {};
    ~StatusCommand() {};
    void printUsage();
    void doCommand(int argc, char **argv);
};

#endif /* _STATUS_H */
