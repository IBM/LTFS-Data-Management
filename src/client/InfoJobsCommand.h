#pragma once

class InfoJobsCommand : public OpenLTFSCommand

{
private:
	void talkToBackend(std::stringstream *parmList) {}
public:
    InfoJobsCommand() : OpenLTFSCommand("jobs", ":+hn:") {};
    ~InfoJobsCommand() {};
    void printUsage();
    void doCommand(int argc, char **argv);
};
