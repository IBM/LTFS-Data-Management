#pragma once

class InfoRequestsCommand : public OpenLTFSCommand

{
private:
	void talkToBackend(std::stringstream *parmList) {}
public:
    InfoRequestsCommand() : OpenLTFSCommand("requests", ":+hwn:") {};
    ~InfoRequestsCommand() {};
    void printUsage();
    void doCommand(int argc, char **argv);
};
