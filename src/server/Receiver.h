#ifndef _RECEIVER_H
#define _RECEIVER_H

extern std::atomic<long> globalReqNumber;

class Receiver

{
public:
	Receiver() {}
	~Receiver() {};
	void run(std::string label, long key);
};

#endif /* _RECEIVER_H */
