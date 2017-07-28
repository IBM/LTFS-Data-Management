#pragma once

class VersionCommand: public OpenLTFSCommand

{
private:
	void talkToBackend(std::stringstream *parmList);
public:
	VersionCommand() :
			OpenLTFSCommand("version", "h")
	{
	}
	~VersionCommand()
	{
	}
	void printUsage();
	void doCommand(int argc, char **argv);
};
