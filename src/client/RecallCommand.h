#ifndef _RECALL_H
#define _RECALL_H

class RecallCommand : public OpenLTFSCommand

{
private:
	void checkOptions(int argc, char **argv) {}
	void talkToBackend(std::stringstream *parmList) {}
public:
    RecallCommand() : OpenLTFSCommand("recall", ":+hwrn:f:R:") {};
    ~RecallCommand() {};
    void printUsage();
    void doCommand(int argc, char **argv);
};

#endif /* _RECALL_H */
