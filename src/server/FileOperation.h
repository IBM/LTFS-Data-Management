#pragma once

class FileOperation {
public:
	FileOperation() {}
	virtual ~FileOperation() = default;
	virtual void addJob(std::string fileName) {}
	virtual void start() {}
	bool queryResult(long reqNumber, long *resident, long *premigrated, long *migrated, long *failed);
};
