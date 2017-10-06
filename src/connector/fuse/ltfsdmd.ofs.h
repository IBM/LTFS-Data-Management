#pragma once

struct fuid_t
{
    unsigned long long fsid;
    unsigned int igen;
    unsigned long long ino;
    friend bool operator<(const fuid_t fuid1, const fuid_t fuid2)
    {
        return (fuid1.ino < fuid2.ino)
                || ((fuid1.ino == fuid2.ino)
                        && ((fuid1.igen < fuid2.igen)
                                || ((fuid1.igen == fuid2.igen)
                                        && ((fuid1.fsid < fuid2.fsid)))));
    }

    friend bool operator==(const fuid_t fuid1, const fuid_t fuid2)
    {
        return (fuid1.ino == fuid2.ino) && (fuid1.igen == fuid2.igen)
                && (fuid1.fsid == fuid2.fsid);
    }
    friend bool operator!=(const fuid_t fuid1, const fuid_t fuid2)
    {
        return !(fuid1 == fuid2);
    }
};

class FuseFS
{
public:
    struct mig_info
    {
        enum state_num
        {
            NO_STATE = 0,
            IN_MIGRATION = 1,
            PREMIGRATED = 2,
            STUBBING = 3,
            MIGRATED = 4,
            IN_RECALL = 5
        } state;
        struct stat statinfo;
        struct timespec changed;
    };
    struct FuseHandle
    {
        char fusepath[PATH_MAX];
        char mountpoint[PATH_MAX];
        char lockpath[PATH_MAX];
        int fd;
        int ffd;
        int lockfd;
    };

private:
    struct ltfsdm_file_info
    {
        int fd;
        int lfd;
        std::string fusepath;
    };

    struct ltfsdm_dir_info
    {
        DIR *dir;
        struct dirent *dentry;
        off_t offset;
    };

    static thread_local std::string lsourcedir;
    std::string mountpt;
    std::thread *thrd;
    char tmpdir[PATH_MAX];
    struct
    {
        bool TEMP_CREATED;
        bool TEMP_MOUNTED;
        bool FUSE_STARTED;
        bool CACHE_MOUNTED;
        bool ROOTFD_FUSE;
    } init_status;

    static const char *relPath(const char *path);
    static std::string lockPath(const char *path);
    static bool needsRecovery(FuseFS::mig_info miginfo);
    static void recoverState(const char *path,
            FuseFS::mig_info::state_num state);
    static int recall_file(FuseFS::ltfsdm_file_info *linfo, bool toresident);
    static bool procIsOpenLTFS(pid_t tid);

    // FUSE call backs
    static int ltfsdm_getattr(const char *path, struct stat *statbuf);
    static int ltfsdm_access(const char *path, int mask);
    static int ltfsdm_readlink(const char *path, char *buffer, size_t size);
    static int ltfsdm_opendir(const char *path, struct fuse_file_info *finfo);
    static int ltfsdm_readdir(const char *path, void *buf,
            fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *finfo);
    static int ltfsdm_releasedir(const char *path,
            struct fuse_file_info *finfo);
    static int ltfsdm_mknod(const char *path, mode_t mode, dev_t rdev);
    static int ltfsdm_mkdir(const char *path, mode_t mode);
    static int ltfsdm_unlink(const char *path);
    static int ltfsdm_rmdir(const char *path);
    static int ltfsdm_symlink(const char *target, const char *linkpath);
    static int ltfsdm_rename(const char *oldpath, const char *newpath);
    static int ltfsdm_link(const char *oldpath, const char *newpath);
    static int ltfsdm_chmod(const char *path, mode_t mode);
    static int ltfsdm_chown(const char *path, uid_t uid, gid_t gid);
    static int ltfsdm_truncate(const char *path, off_t size);
    static int ltfsdm_utimens(const char *path, const struct timespec times[2]);
    static int ltfsdm_open(const char *path, struct fuse_file_info *finfo);
    static int ltfsdm_ftruncate(const char *path, off_t size,
            struct fuse_file_info *finfo);
    static int ltfsdm_read(const char *path, char *buffer, size_t size,
            off_t offset, struct fuse_file_info *finfo);
    static int ltfsdm_read_buf(const char *path, struct fuse_bufvec **bufferp,
            size_t size, off_t offset, struct fuse_file_info *finfo);
    static int ltfsdm_write(const char *path, const char *buf, size_t size,
            off_t offset, struct fuse_file_info *finfo);
    static int ltfsdm_write_buf(const char *path, struct fuse_bufvec *buf,
            off_t offset, struct fuse_file_info *finfo);
    static int ltfsdm_statfs(const char *path, struct statvfs *stbuf);
    static int ltfsdm_release(const char *path, struct fuse_file_info *finfo);
    static int ltfsdm_flush(const char *path, struct fuse_file_info *finfo);
    static int ltfsdm_fsync(const char *path, int isdatasync,
            struct fuse_file_info *finfo);
    static int ltfsdm_fallocate(const char *path, int mode, off_t offset,
            off_t length, struct fuse_file_info *finfo);
    static int ltfsdm_setxattr(const char *path, const char *name,
            const char *value, size_t size, int flags);
    static int ltfsdm_getxattr(const char *path, const char *name, char *value,
            size_t size);
    static int ltfsdm_listxattr(const char *path, char *list, size_t size);
    static int ltfsdm_removexattr(const char *path, const char *name);
    static int ltfsdm_ioctl(const char *path, int cmd, void *arg,
            struct fuse_file_info *fi, unsigned int flags, void *data);
    static void *ltfsdm_init(struct fuse_conn_info *conn);

    static void execute(std::string sourcedir, std::string mountpt,
            std::string command);

public:
    static Connector::rec_info_t recinfo_share;
    static std::atomic<fuid_t> trecall_fuid;
    static std::atomic<bool> no_rec_event;
    static std::atomic<long> ltfsdmKey;
    static std::string command;
    static std::atomic<int> rootFd;
    static std::atomic<int> lockFd;
    static std::atomic<int> ioctlFd;
    static std::atomic<pid_t> mainpid;
    static std::mutex lockmtx;
    enum
    {
        LTFSDM_FINFO = _IOR('l', 0, FuseFS::FuseHandle), LTFSDM_PREMOUNT = _IO(
                'l', 1), LTFSDM_POSTMOUNT = _IO('l', 2), LTFSDM_STOP = _IO('l',
                3),
    };

    static FuseFS::mig_info genMigInfoAt(int fd,
            FuseFS::mig_info::state_num state);
    static FuseFS::mig_info genMigInfo(std::string fileName,
            FuseFS::mig_info::state_num state);
    static void setMigInfoAt(int fd, FuseFS::mig_info::state_num state);
    static void setMigInfo(std::string fileName,
            FuseFS::mig_info::state_num state);
    static int remMigInfoAt(int fd);
    static FuseFS::mig_info getMigInfoAt(int fd);
    static struct fuse_operations init_operations();
    std::string getMountPoint()
    {
        return mountpt;
    }
    FuseFS(std::string _mountpt) :
            mountpt(_mountpt), thrd(nullptr), init_status( { false, false,
                    false, false, false })
    {
    }
    void init(struct timespec starttime);
    ~FuseFS();
};

struct conn_info_t
{
    LTFSDmCommServer *reqrequest;
};
