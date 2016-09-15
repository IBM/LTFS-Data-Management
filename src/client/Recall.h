#ifndef _RECALL_H
#define _RECALL_H

class Recall : public Operation

{
private:
	bool waitForCompletion;
	bool preMigrate;
	long requestNumber;
	std::string fileList;
	std::string directoryName;
public:
	Recall();
    ~Recall() {};
    void printUsage();
    void doOperation(int argc, char **argv);
};

#endif /* _RECALL_H */
