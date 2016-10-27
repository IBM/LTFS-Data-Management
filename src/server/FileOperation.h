#ifndef _FILEOPERATION_H
#define _FILEOPERATION_H

class FileOperation {
public:
	virtual void addFileName(std::string fileName) {}
	virtual void start() {}
	bool queryResult(long reqNumber, long *resident, long *premigrated, long *migrated);
};


#endif /* _FILEOPERATION_H */
