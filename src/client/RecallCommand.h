#ifndef _RECALL_H
#define _RECALL_H

class RecallCommand : public OpenLTFSCommand

{
private:
public:
    RecallCommand() : OpenLTFSCommand("+hwrn:f:R:") {};
    ~RecallCommand() {};
    void printUsage();
    void doCommand(int argc, char **argv);
    static constexpr const char *cmd1 = "recall";
    static constexpr const char *cmd2 = "";
};

#endif /* _RECALL_H */
