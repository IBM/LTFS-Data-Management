#pragma once

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
