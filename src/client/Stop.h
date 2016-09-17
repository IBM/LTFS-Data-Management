#ifndef _STOP_H
#define _STOP_H

class Stop : public Operation

{
public:
    Stop() : Operation("") {};
    ~Stop() {};
    void printUsage();
    void doOperation(int argc, char **argv);
 	static constexpr const char *cmd1 = "stop";
	static constexpr const char *cmd2 = "";
};

#endif /* _STOP_H */
