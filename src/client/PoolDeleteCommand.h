#ifndef _POOL_DELETE_COMMAND_H
#define _POOL_DELETE_COMMAND_H

class PoolDeleteCommand : public OpenLTFSCommand

{
private:
	void talkToBackend(std::stringstream *parmList) {}
public:
    PoolDeleteCommand() : OpenLTFSCommand("delete", ":+hP:") {};
    ~PoolDeleteCommand() {};
    void printUsage();
    void doCommand(int argc, char **argv);
};

#endif /* _POOL_DELETE_COMMAND_H */
