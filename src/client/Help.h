#ifndef _HELP_H
#define _HELP_H

class Help : public Operation

{
public:
	Help();
    ~Help() {};
    void printUsage();
    void doOperation(int argc, char **argv);
};

#endif /* _HELP_H */
