#ifndef _RECALL_H
#define _RECALL_H

class RecallCommand : public OpenLTFSCommand

{
private:
	void talkToBackend(std::stringstream *parmList);
public:
    RecallCommand() : OpenLTFSCommand("recall", ":+hwrn:f:") {};
    ~RecallCommand() {};
    void printUsage();
    void doCommand(int argc, char **argv);
};

#endif /* _RECALL_H */
