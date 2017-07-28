#pragma once

class AddCommand: public OpenLTFSCommand

{
private:
	void talkToBackend(std::stringstream *parmList)
	{
	}
public:
	AddCommand() :
			OpenLTFSCommand("add", ":+m:N:")
	{
	}
	~AddCommand()
	{
	}
	void printUsage();
	void doCommand(int argc, char **argv);
};
