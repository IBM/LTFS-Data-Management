#ifndef _POOL_ADD_COMMAND_H
#define _POOL_ADD_COMMAND_H

class PoolAddCommand : public OpenLTFSCommand

{
private:
	void talkToBackend(std::stringstream *parmList) {}
public:
    PoolAddCommand() : OpenLTFSCommand("add", ":+hP:t:") {};
    ~PoolAddCommand() {};
    void printUsage();
    void doCommand(int argc, char **argv);
};

#endif /* _POOL_ADD_COMMAND_H */
