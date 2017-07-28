#pragma once

class RecallCommand: public OpenLTFSCommand

{
private:
	void talkToBackend(std::stringstream *parmList);
public:
	RecallCommand() :
			OpenLTFSCommand("recall", ":+hrn:f:")
	{
	}
	~RecallCommand()
	{
	}
	void printUsage();
	void doCommand(int argc, char **argv);
};
