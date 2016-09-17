#ifndef _OPERATION_H
#define _OPERATION_H

class OpenLTFSCommand
{
protected:
    OpenLTFSCommand(std::string command_, std::string optionStr_) :
        waitForCompletion(false),
		preMigrate(false),
		recToResident(false),
		requestNumber(-1),
		collocationFactor(1),
		fileList(""),
		directoryName(""),
	    command(command_),
		optionStr(optionStr_) {};
	bool waitForCompletion;
	bool preMigrate;
	bool recToResident;
	long requestNumber;
	unsigned long collocationFactor;
	std::string fileList;
	std::string directoryName;
	std::string command;
	std::string optionStr;

public:
    virtual ~OpenLTFSCommand() {};
    virtual void printUsage() = 0;
    virtual void doCommand(int argc, char **argv) = 0;

	// non-virtual functions
	void processOptions(int argc, char **argv);
	bool compare(std::string name) { return !command.compare(name); }
};

#endif /* _OPERATION_H */
