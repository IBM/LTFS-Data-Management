#ifndef _HELP_H
#define _HELP_H

class HelpCommand : public OpenLTFSCommand

{
public:
    HelpCommand() : OpenLTFSCommand("") {};
    ~HelpCommand() {};
    void printUsage();
    void doCommand(int argc, char **argv);
 	static constexpr const char *command = "help";
};

#endif /* _HELP_H */
