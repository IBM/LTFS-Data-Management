#ifndef _CONNECTOR_H
#define _CONNECTOR_H

class Connector {
public:
	Connector(bool cleanup);
	~Connector();
};

class FsObj {
private:
	void *handle;
	unsigned long handleLength;
	bool isLocked;
public:
	struct attr_t {
		unsigned long typeId;
		int copies;
		char tapeId[Const::maxReplica][Const::tapeIdLength];
	};
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
	long write(long offset, unsigned long size, char *buffer);
	void addAttribute(attr_t value);
	void remAttribute();
	attr_t getAttribute();
	void finishPremigration();
	void finishRecall(file_state fstate);
	void prepareStubbing();
	void stub();
	file_state getMigState();
};

#endif /* _CONNECTOR_H */
