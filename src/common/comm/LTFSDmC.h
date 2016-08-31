#ifndef _COMM_H
#define _COMM_H

namespace LTFSDmD {
class LTFSDmC : public LTFSDmD::Command {
public:
	int send(int fd);
	int recv(int fd);
};
}

#endif /* _COMM_H */
