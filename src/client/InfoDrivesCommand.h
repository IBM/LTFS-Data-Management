#pragma once

class InfoDrivesCommand: public OpenLTFSCommand

{
private:
	void talkToBackend(std::stringstream *parmList)
	{
	}
public:
	InfoDrivesCommand() :
			OpenLTFSCommand("drives", ":+h")
	{
	}
	;
	~InfoDrivesCommand()
	{
	}
	;
	void printUsage();
	void doCommand(int argc, char **argv);
};
