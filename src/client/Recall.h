#ifndef _RECALL_H
#define _RECALL_H

class Recall : public Operation

{
private:
public:
	Recall() {};
    ~Recall() {};
    void printUsage();
    void doOperation(int argc, char **argv);
};

#endif /* _RECALL_H */
