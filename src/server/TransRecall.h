#ifndef _TRANSRECALL_H
#define _TRANSRECALL_H

class TransRecall

{
public:
	TransRecall() {}
	~TransRecall() {};
	static void recall(Connector::rec_info_t recinfo, std::string tapeId, long reqNum, bool newReq);
	void run(Connector *connector);
	static void execRequest(int reqNum, std::string tapeId);
};


#endif /* _TRANSRECALL_H */
