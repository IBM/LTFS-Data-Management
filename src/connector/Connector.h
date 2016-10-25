#ifndef _CONNECTOR_H
#define _CONNECTOR_H

class Connector {
public:
	Connector();
	~Connector();
};

class FsObj {
private:
	void *handle;
	unsigned long handleLength;
	bool isLocked;
	long offset;
public:
	FsObj(std::string fileName);
	FsObj(unsigned long long fsId, unsigned int iGen, unsigned long long iNode);
	~FsObj();
	struct stat stat();
	unsigned long long getFsId();
	unsigned int getIGen();
	unsigned long long getINode();
	std::string getTapeId();
	void lock();
	void unlock();
	bool read(unsigned long size, char *buffer, long *rsize);
	void addAttribute(std::string key, std::string value);
	void finishPremigration();
	void stub();
};

#endif /* _CONNECTOR_H */
