#ifndef _OPERATION_H
#define _OPERATION_H

class OpenLTFSCommand
{
protected:
    OpenLTFSCommand(std::string command_, std::string optionStr_) :
        waitForCompletion(false),
		preMigrate(false),
		recToResident(false),
		requestNumber(Const::UNSET),
		collocationFactor(1),
		fileList(""),
		numReplica(1),
	    command(command_),
		optionStr(optionStr_),
		fsName(""),
		mountPoint(""),
		startTime(time(NULL)),
		poolName(""),
		tapeList({}),
	    key(Const::UNSET) {};
	bool waitForCompletion;
	bool preMigrate;
	bool recToResident;
	long requestNumber;
	unsigned long collocationFactor;
	std::string fileList;
	long numReplica;
	std::string command;
	std::string optionStr;
	std::string fsName;
	std::string mountPoint;
	std::ifstream fileListStrm;
	time_t startTime;
	std::string poolName;
	std::list<std::string> tapeList;
	long key;
	LTFSDmCommClient commCommand;

	void getRequestNumber();
	void queryResults();

	void checkOptions(int argc, char **argv);
 	virtual void talkToBackend(std::stringstream *parmList) {}

public:
    virtual ~OpenLTFSCommand();
    virtual void printUsage() = 0;
    virtual void doCommand(int argc, char **argv) = 0;

	// non-virtual methods
	void processOptions(int argc, char **argv);
	void traceParms();
	bool compare(std::string name) { return !command.compare(name); }
	void connect();
	void sendObjects(std::stringstream *parmList);
	void isValidRegularFile();
};

#endif /* _OPERATION_H */
