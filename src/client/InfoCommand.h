#pragma once

class InfoCommand: public OpenLTFSCommand

{
private:
	void talkToBackend(std::stringstream *parmList)
	{
	}
public:
	InfoCommand() :
			OpenLTFSCommand("info", "")
	{
	}
	void printUsage()
	{
		INFO(LTFSDMC0020I);
	}
	;
	void doCommand(int argc, char **argv)
	{
	}
};
