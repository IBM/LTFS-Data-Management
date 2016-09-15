#ifndef _MIGRATION_H
#define _MIGRATION_H

class Migration : public Operation

{
private:
	bool waitForCompletion;
	bool preMigrate;
	long requestNumber;
	unsigned long collocationFactor;
	std::string fileList;
	std::string directoryName;
public:
	Migration();
    ~Migration() {};
    void printUsage();
    void doOperation(int argc, char **argv);
};

#endif /* _MIGRATION_H */
