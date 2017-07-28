#pragma once

class InfoPoolsCommand: public OpenLTFSCommand

{
private:
	void talkToBackend(std::stringstream *parmList)
	{
	}
public:
	InfoPoolsCommand() :
			OpenLTFSCommand("pools", ":+h")
	{
	}
	~InfoPoolsCommand()
	{
	}
	void printUsage();
	void doCommand(int argc, char **argv);
};
