#ifndef _STOP_H
#define _STOP_H

class StopCommand : public OpenLTFSCommand

{
public:
    StopCommand() : OpenLTFSCommand("") {};
    ~StopCommand() {};
    void printUsage();
    void doCommand(int argc, char **argv);
 	static constexpr const char *cmd1 = "stop";
	static constexpr const char *cmd2 = "";
};

#endif /* _STOP_H */
