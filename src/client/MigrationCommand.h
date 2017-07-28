#pragma once

class MigrationCommand: public OpenLTFSCommand

{
private:
	void talkToBackend(std::stringstream *parmList);
public:
	MigrationCommand() :
			OpenLTFSCommand("migrate", ":+hpn:f:P:")
	{
	}
	~MigrationCommand()
	{
	}
	void printUsage();
	void doCommand(int argc, char **argv);
};
