#ifndef _FUSEFS_H
#define _FUSEFS_H

extern std::mutex trecall_mtx;
extern std::condition_variable trecall_cond;
extern std::mutex trecall_reply_mtx;
extern std::condition_variable trecall_reply_cond;
extern std::condition_variable trecall_reply_wait_cond;
extern std::atomic<unsigned long> trecall_ino;
extern Connector::rec_info_t recinfo_share;
extern std::condition_variable wait_cond;
extern std::atomic<bool> single;

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
	char sourcedir[PATH_MAX];
	char mountpoint[PATH_MAX];
	struct timespec starttime;
};

class FuseFS {
private:
	std::thread *fusefs;
	struct openltfs_ctx_t *ctx;
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
