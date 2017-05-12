#ifndef _POOL_CREATE_COMMAND_H
#define _POOL_CREATE_COMMAND_H

class PoolCreateCommand : public OpenLTFSCommand

{
private:
	void talkToBackend(std::stringstream *parmList) {}
public:
    PoolCreateCommand() : OpenLTFSCommand("create", ":+hP:") {};
    ~PoolCreateCommand() {};
    void printUsage();
    void doCommand(int argc, char **argv);
};

#endif /* _POOL_CREATE_COMMAND_H */
