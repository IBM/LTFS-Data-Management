#pragma once

class StopCommand : public OpenLTFSCommand

{
private:
	void talkToBackend(std::stringstream *parmList) {}
public:
    StopCommand() : OpenLTFSCommand("stop", "") {};
    ~StopCommand() {};
    void printUsage();
    void doCommand(int argc, char **argv);
};
