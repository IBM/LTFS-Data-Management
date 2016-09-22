#ifndef _INFO_H
#define _INFO_H

class InfoCommand : public OpenLTFSCommand

{
private:
public:
	InfoCommand() : OpenLTFSCommand("info", "") {};
	void printUsage() { MSG_INFO(LTFSDMC0020I); };
    void doCommand(int argc, char **argv) {};
};

#endif /* _INFO_H */
