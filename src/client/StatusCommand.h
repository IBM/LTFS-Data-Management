#pragma once

class StatusCommand : public OpenLTFSCommand

{
private:
	std::stringstream serverPath;
	void determineServerPath();
	void talkToBackend(std::stringstream *parmList) {}
public:
    StatusCommand() : OpenLTFSCommand("status", "h") {};
    ~StatusCommand() {};
    void printUsage();
    void doCommand(int argc, char **argv);
};
