#ifndef _START_H
#define _START_H

class StartCommand : public OpenLTFSCommand

{
public:
    StartCommand() : OpenLTFSCommand("") {};
    ~StartCommand() {};
    void printUsage();
    void doCommand(int argc, char **argv);
	static constexpr const char *command = "start";
};

#endif /* _START_H */
