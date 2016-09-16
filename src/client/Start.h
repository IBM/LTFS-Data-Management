#ifndef _START_H
#define _START_H

class Start : public Operation

{
public:
    Start() : Operation("") {};
    ~Start() {};
    void printUsage();
    void doOperation(int argc, char **argv);
};

#endif /* _START_H */
