#pragma once

#ifndef TRUE
#define TRUE 1
#endif

#ifndef FALSE
#define FALSE 0
#endif

class FileSystems
{
public:
    struct fsinfo
    {
        std::string source;
        std::string target;
        std::string fstype;
        std::string uuid;
        std::string options;
    };
private:
    bool first;
    struct libmnt_context *cxt; // = mnt_new_context();
    struct libmnt_table *tb;
    blkid_cache cache;
    fsinfo getContext(struct libmnt_fs *mntfs);
    void getTable();
public:
    enum umountflag
    {
        UMNT_NORMAL, UMNT_DETACHED, UMNT_FORCED, UMNT_DETACHED_FORCED,
    };
    enum mountflag
    {
        MNT_NORMAL, MNT_FAKE,
    };
    FileSystems();
    ~FileSystems();
    std::vector<FileSystems::fsinfo> getAll();
    FileSystems::fsinfo getByTarget(std::string target);
    void mount(std::string source, std::string target, std::string options, mountflag flag);
    void umount(std::string target, umountflag flag);
};
