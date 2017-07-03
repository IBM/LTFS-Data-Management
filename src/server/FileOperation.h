#pragma once

class FileOperation {
public:
	virtual void addJob(std::string fileName) {}
	virtual void start() {}
	bool queryResult(long reqNumber, long *resident, long *premigrated, long *migrated, long *failed);
};
