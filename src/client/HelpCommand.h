#ifndef _HELP_H
#define _HELP_H

class HelpCommand : public OpenLTFSCommand

{
public:
    HelpCommand() : OpenLTFSCommand("") {};
    ~HelpCommand() {};
    void printUsage();
    void doCommand(int argc, char **argv);
 	static constexpr const char *cmd1 = "help";
	static constexpr const char *cmd2 = "";
};

#endif /* _HELP_H */
