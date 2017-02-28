#ifndef _FUSEFS_H
#define _FUSEFS_H

struct mig_info_t {
	enum state_t {
		NO_STATE = 0,
		IN_MIGRATION = 1,
		PREMIGRATED = 2,
		STUBBING = 3,
		MIGRATED = 4,
		IN_RECALL = 5
	};
	state_t state;
	struct stat statinfo;
	struct timespec changed;
};

mig_info_t genMigInfo(const char *path, mig_info_t::state_t state);
void setMigInfo(const char *path, mig_info_t::state_t state);
void remMigInfo(const char *path);
mig_info_t getMigInfo(const char *path);
mig_info_t getMigInfoAt(int dirfd, const char *path);
bool needsRecovery(mig_info_t miginfo);
void recoverState(const char *path, mig_info_t::state_t state);


struct openltfs_ctx_t {
	const char *sourcedir;
	struct timespec starttime;
};

class FuseFS {
private:
	std::thread *fusefs;
	struct openltfs_ctx_t ctx;
	std::string mountpt;
	struct fuse_chan *openltfsch = NULL;
	struct fuse *openltfs = NULL;
	struct fuse_operations init_operations();
public:
	std::string getMountPoint() {return mountpt;}
	FuseFS(std::string sourcedir, std::string mountpt, std::string fsName, struct timespec starttime);
	~FuseFS();
};

#endif /* _FUSEFS_H */
