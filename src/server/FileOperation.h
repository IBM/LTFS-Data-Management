#pragma once

class FileOperation {
protected:
	static std::string genInumString(std::list<unsigned long> inumList);
	static const std::string REQUEST_STATE;
	static const std::string DELETE_JOBS;
	static const std::string DELETE_REQUESTS;
public:
	FileOperation() {}
	virtual ~FileOperation() = default;
	virtual void addJob(std::string fileName) {}
	virtual void start() {}
	bool queryResult(long reqNumber, long *resident, long *premigrated, long *migrated, long *failed);
};
