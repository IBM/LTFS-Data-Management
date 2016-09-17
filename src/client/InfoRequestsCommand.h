#ifndef _INFOREQUESTS_H
#define _INFOREQUESTS_H

class InfoRequestsCommand : public OpenLTFSCommand

{
private:
public:
    InfoRequestsCommand() : OpenLTFSCommand("+hwn:") {};
    ~InfoRequestsCommand() {};
    void printUsage();
    void doCommand(int argc, char **argv);
    static constexpr const char *command = "requests";
};

#endif /* _INFOREQUESTS_H */
