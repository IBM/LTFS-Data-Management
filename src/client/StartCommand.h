#ifndef _START_H
#define _START_H

class StartCommand : public OpenLTFSCommand

{
public:
    StartCommand() : OpenLTFSCommand("start", "") {};
    ~StartCommand() {};
    void printUsage();
    void doCommand(int argc, char **argv);
};

#endif /* _START_H */
