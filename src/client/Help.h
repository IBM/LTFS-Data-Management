#ifndef _HELP_H
#define _HELP_H

class Help : public Operation

{
public:
    Help() : Operation("") {};
    ~Help() {};
    void printUsage();
    void doOperation(int argc, char **argv);
 	static constexpr const char *cmd1 = "help";
	static constexpr const char *cmd2 = "";
};

#endif /* _HELP_H */
