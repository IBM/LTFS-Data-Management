#ifndef _INFOREQUESTS_H
#define _INFOREQUESTS_H

class InfoRequests : public Operation

{
private:
public:
	InfoRequests() {};
    ~InfoRequests() {};
    void printUsage();
    void doOperation(int argc, char **argv);
};

#endif /* _INFOREQUESTS_H */
