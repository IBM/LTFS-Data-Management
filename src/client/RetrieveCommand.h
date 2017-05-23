#ifndef _RETRIEVE_H
#define _RETRIEVE_H

class RetrieveCommand : public OpenLTFSCommand

{
private:
	std::stringstream serverPath;
	void determineServerPath();
	void talkToBackend(std::stringstream *parmList) {}
public:
    RetrieveCommand() : OpenLTFSCommand("retrieve", "h") {};
    ~RetrieveCommand() {};
    void printUsage();
    void doCommand(int argc, char **argv);
};

#endif /* _RETRIEVE_H */
