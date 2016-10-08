#ifndef _STOP_H
#define _STOP_H

class StopCommand : public OpenLTFSCommand

{
private:
	void talkToBackend(std::stringstream *parmList) {}
public:
    StopCommand() : OpenLTFSCommand("stop", "") {};
    ~StopCommand() {};
    void printUsage();
    void doCommand(int argc, char **argv);
};

#endif /* _STOP_H */
