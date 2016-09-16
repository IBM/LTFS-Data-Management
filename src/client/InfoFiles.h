#ifndef _INFOFILES_H
#define _INFOFILES_H

class InfoFiles : public Operation

{
private:
public:
	InfoFiles() {};
    ~InfoFiles() {};
    void printUsage();
    void doOperation(int argc, char **argv);
};

#endif /* _INFOFILES_H */
