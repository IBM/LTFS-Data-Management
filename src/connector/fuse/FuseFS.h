#ifndef _FUSEFS_H
#define _FUSEFS_H

class FuseFS {
private:
	std::thread *fusefs;
	std::string sourcedir;
	std::string mountpt;
	struct fuse_chan *openltfsch = NULL;
	struct fuse *openltfs = NULL;
	struct fuse_operations init_operations();
	void run();
	int fd;
public:
	bool isMigrated(int fd);
	std::string getMountPoint() {return mountpt;}
	FuseFS(std::string sourcedir_, std::string mountpt_);
	~FuseFS();
};

#endif /* _FUSEFS_H */
