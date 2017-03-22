#ifndef _INFOJOBS_H
#define _INFOJOBS_H

class InfoJobsCommand : public OpenLTFSCommand

{
private:
	void talkToBackend(std::stringstream *parmList) {}
public:
    InfoJobsCommand() : OpenLTFSCommand("jobs", ":+hn:") {};
    ~InfoJobsCommand() {};
    void printUsage();
    void doCommand(int argc, char **argv);
};

#endif /* _INFOJOBS_H */
