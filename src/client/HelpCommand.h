#pragma once

class HelpCommand: public OpenLTFSCommand

{
private:
	void talkToBackend(std::stringstream *parmList)
	{
	}
public:
	HelpCommand() :
			OpenLTFSCommand("help", "")
	{
	}
	~HelpCommand()
	{
	}
	void printUsage();
	void doCommand(int argc, char **argv);
};
