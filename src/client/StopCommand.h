#ifndef _STOP_H
#define _STOP_H

class StopCommand : public OpenLTFSCommand

{
public:
    StopCommand() : OpenLTFSCommand("stop", "") {};
    ~StopCommand() {};
    void printUsage();
    void doCommand(int argc, char **argv);
};

#endif /* _STOP_H */
