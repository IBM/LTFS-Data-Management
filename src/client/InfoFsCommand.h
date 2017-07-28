#pragma once

class InfoFsCommand: public OpenLTFSCommand

{
private:
	void talkToBackend(std::stringstream *parmList);
public:
	InfoFsCommand() :
			OpenLTFSCommand("fs", "h")
	{
	}
	~InfoFsCommand()
	{
	}
	void printUsage();
	void doCommand(int argc, char **argv);
};
