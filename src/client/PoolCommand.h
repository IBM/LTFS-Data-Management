#ifndef _POOL_H
#define _POOL_H

class PoolCommand : public OpenLTFSCommand

{
private:
	void talkToBackend(std::stringstream *parmList) {}
public:
	PoolCommand() : OpenLTFSCommand("pool", "") {};
	void printUsage() { INFO(LTFSDMC0073I); };
    void doCommand(int argc, char **argv) {};
};

#endif /* _POOL_H */
