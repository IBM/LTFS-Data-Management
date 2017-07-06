#pragma once

class InfoRequestsCommand : public OpenLTFSCommand

{
private:
	void talkToBackend(std::stringstream *parmList) {}
public:
    InfoRequestsCommand() : OpenLTFSCommand("requests", ":+hn:") {};
    ~InfoRequestsCommand() {};
    void printUsage();
    void doCommand(int argc, char **argv);
};
