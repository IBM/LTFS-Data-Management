#ifndef _FILEOPERATION_H
#define _FILEOPERATION_H

class FileOperation {
protected:
	bool needsTape = false;
public:
	virtual void addJob(std::string fileName) {}
	virtual void start() {}
	bool queryResult(long reqNumber, long *resident, long *premigrated, long *migrated, long *failed);
};


#endif /* _FILEOPERATION_H */
