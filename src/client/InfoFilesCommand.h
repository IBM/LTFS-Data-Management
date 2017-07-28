#pragma once

class InfoFilesCommand: public OpenLTFSCommand

{
private:
	void talkToBackend(std::stringstream *parmList);
public:
	InfoFilesCommand() :
			OpenLTFSCommand("files", ":+hf:")
	{
	}
	;
	~InfoFilesCommand()
	{
	}
	;
	void printUsage();
	void doCommand(int argc, char **argv);
};
