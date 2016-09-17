#ifndef _INFOREQUESTS_H
#define _INFOREQUESTS_H

class InfoRequestsCommand : public OpenLTFSCommand

{
private:
public:
    InfoRequestsCommand() : OpenLTFSCommand("requests", "+hwn:") {};
    ~InfoRequestsCommand() {};
    void printUsage();
    void doCommand(int argc, char **argv);
};

#endif /* _INFOREQUESTS_H */
