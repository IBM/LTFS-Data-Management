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
public:
	enum file_state {
		RESIDENT,
		PREMIGRATED,
		MIGRATED
	};
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
	long read(long offset, unsigned long size, char *buffer);
	void addAttribute(std::string key, std::string value);
	std::string getAttribute(std::string key);
	void finishPremigration();
	void prepareStubbing();
	void stub();
	file_state getMigState();
};

#endif /* _CONNECTOR_H */
