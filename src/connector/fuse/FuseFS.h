#ifndef _FUSEFS_H
#define _FUSEFS_H

class FuseFS {
private:
	std::string mountpt;
	struct fuse_chan *openltfsch = NULL;
	struct fuse *openltfs = NULL;
	std::thread *fusefs;

public:
	struct fuse_operations init_operations();

public:
	bool isMigrated(int fd);
	FuseFS();
	~FuseFS();
};

#endif /* _FUSEFS_H */
