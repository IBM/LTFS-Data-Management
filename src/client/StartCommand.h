#ifndef _START_H
#define _START_H

class StartCommand : public OpenLTFSCommand

{
private:
	std::stringstream serverPath;
	void determineServerPath();
	void startServer();
	void talkToBackend(std::stringstream *parmList) {}
public:
    StartCommand() : OpenLTFSCommand("start", "") {};
    ~StartCommand() {};
    void printUsage();
    void doCommand(int argc, char **argv);
};

#endif /* _START_H */
