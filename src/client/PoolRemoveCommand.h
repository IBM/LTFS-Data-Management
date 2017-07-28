#pragma once

class PoolRemoveCommand: public OpenLTFSCommand

{
private:
	void talkToBackend(std::stringstream *parmList)
	{
	}
public:
	PoolRemoveCommand() :
			OpenLTFSCommand("remove", ":+hP:t:")
	{
	}
	;
	~PoolRemoveCommand()
	{
	}
	;
	void printUsage();
	void doCommand(int argc, char **argv);
};
