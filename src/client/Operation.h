#ifndef _OPERATION_H
#define _OPERATION_H

class Operation
{
protected:
    Operation() : waitForCompletion(false),
		preMigrate(false),
		requestNumber(-1),
		collocationFactor(1),
		fileList(""),
		directoryName("") {};
	bool waitForCompletion;
	bool preMigrate;
	long requestNumber;
	unsigned long collocationFactor;
	std::string fileList;
	std::string directoryName;

public:
    virtual ~Operation() {};
    virtual void printUsage() = 0;
    virtual void doOperation(int argc, char **argv) = 0;
};

#endif /* _OPERATION_H */
