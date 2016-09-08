#ifndef _OPERATION_H
#define _OPERATION_H

class Operation
{
protected:
	Operation() {};
public:
    virtual ~Operation() {};
    virtual void printUsage() = 0;
    virtual void doOperation(int argc, char **argv) = 0;
};

#endif /* _OPERATION_H */
