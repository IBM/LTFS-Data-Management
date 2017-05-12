#ifndef _POOL_REMOVE_COMMAND_H
#define _POOL_REMOVE_COMMAND_H

class PoolRemoveCommand : public OpenLTFSCommand

{
private:
	void talkToBackend(std::stringstream *parmList) {}
public:
    PoolRemoveCommand() : OpenLTFSCommand("remove", ":+hP:t:") {};
    ~PoolRemoveCommand() {};
    void printUsage();
    void doCommand(int argc, char **argv);
};

#endif /* _POOL_REMOVE_COMMAND_H */
