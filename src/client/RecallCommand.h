#ifndef _RECALL_H
#define _RECALL_H

class RecallCommand : public OpenLTFSCommand

{
private:
public:
    RecallCommand() : OpenLTFSCommand("recall", "+hwrn:f:R:") {};
    ~RecallCommand() {};
    void printUsage();
    void doCommand(int argc, char **argv);
};

#endif /* _RECALL_H */
