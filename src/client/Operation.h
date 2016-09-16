#ifndef _OPERATION_H
#define _OPERATION_H

class Operation
{
protected:
    Operation(std::string optionStr_) :
        waitForCompletion(false),
		preMigrate(false),
		recToResident(false),
		requestNumber(-1),
		collocationFactor(1),
		fileList(""),
		directoryName(""),
		optionStr(optionStr_) {};
	bool waitForCompletion;
	bool preMigrate;
	bool recToResident;
	long requestNumber;
	unsigned long collocationFactor;
	std::string fileList;
	std::string directoryName;
	std::string optionStr;

public:
    virtual ~Operation() {};
    virtual void printUsage() = 0;
    virtual void doOperation(int argc, char **argv) = 0;

	// non-virtual functions
	void processOptions(int argc, char **argv);
};

#endif /* _OPERATION_H */
