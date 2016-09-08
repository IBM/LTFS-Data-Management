#ifndef _LIST_H
#define _LIST_H

class List : public Operation

{
public:
	List();
    ~List() {};
    void printUsage();
    void doOperation(int argc, char **argv);
};

#endif /* _LIST_H */
