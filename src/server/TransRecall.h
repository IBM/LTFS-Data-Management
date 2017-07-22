#pragma once

class TransRecall

{
public:
	TransRecall() {}
	~TransRecall() {};
	static void addRequest(Connector::rec_info_t recinfo, std::string tapeId, long reqNum);
	void cleanupEvents();
	void run(Connector *connector);
	static void execRequest(int reqNum, std::string tapeId);
};
