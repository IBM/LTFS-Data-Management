#ifndef _INFO_FILES_H
#define _INFO_FILES_H

class InfoFiles : public FileOperation {
private:
	unsigned long pid;
	long reqNumber;
public:
	InfoFiles(unsigned long _pid, long _reqNumber) : pid(_pid), reqNumber(_reqNumber) {}
	void start();
};

#endif /* _INFO_FILES_H */
