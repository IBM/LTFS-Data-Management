#ifndef _STOP_H
#define _STOP_H

class Stop : public Operation

{
public:
    Stop() : Operation("") {};
    ~Stop() {};
    void printUsage();
    void doOperation(int argc, char **argv);
};

#endif /* _STOP_H */
