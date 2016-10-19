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
	unsigned long size;
public:
	FsObj(std::string fileName);
	FsObj(unsigned long long fsId, unsigned int iGen, unsigned long long iNode);
	~FsObj();
	struct stat stat();
	unsigned long getFsId();
	unsigned long getIGen();
	unsigned long getINode();
};

#endif /* _CONNECTOR_H */
