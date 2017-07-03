#pragma once

class RetrieveCommand : public OpenLTFSCommand

{
private:
	std::stringstream serverPath;
	void determineServerPath();
	void talkToBackend(std::stringstream *parmList) {}
public:
    RetrieveCommand() : OpenLTFSCommand("retrieve", "h") {};
    ~RetrieveCommand() {};
    void printUsage();
    void doCommand(int argc, char **argv);
};
