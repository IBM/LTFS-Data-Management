#pragma once

/** @page connector Connector

    # Connector

    LTFS Data Management provides three possibilities to move data between
    disk and tape:

    operation | explanation
    ---|---
    migration | moves data from disk to tape
    selective recall | selectively moves data from tape to disk
    transparent recall | transparently moved data from tape to disk

    Selective recall moves data on request. I.e. a file is known as migrated
    and a user wants to recall it back to disk. Transparent recall does the
    same but on data access. I.e. a user does not need to be aware that data
    is on tape. If data is e.g. read the read operation is blocked for the
    time data is transferred back to disk. For selective recall applications
    need to be aware that a file is migrated and need to initiate the recall
    operation. For transparent recall applications do not need to be aware
    of the migration state. Read, write, and truncate system calls are
    blocked as long as it take to move data back to disk. In this case
    applications do not have to be rewritten to deal with migrated data but
    should be tolerant against delays since it can take time to perform the
    data transfer.

    ## Three different technologies for data management

    To block system calls require additional functionality provided by the
    operating system. Migration and selective recall do not have any additional
    requirements on the operating system and therefore are simpler and more
    flexible to implement. Three different technologies have been discussed
    or implemented for that purpose:

    technology/API  | description
    ---|---
    DMAPI | Data Management API (https://en.wikipedia.org/wiki/DMAPI), only available for the XFS file system type
    Fanotify | File system event notification system (http://man7.org/linux/man-pages/man7/fanotify.7.html), file system independent, no mandatory locking
    @ref fuse_connector "Fuse overlay file system"| File system in user space (https://github.com/libfuse/libfuse), file system independent, no direct file system access

    The Fanotify API looked very promising since it is file system independent
    and also provides direct file system access. This API seems to have two
    limitations that still persist:

    - Only open (FAN_OPEN_PERM) and read (FAN_ACCESS_PERM) calls are able to
      intercept. It is possible to deal with that limitation by using the
      assumption that a file opened for write will be always written. If an
      application opens a file for writing it would be always recalled even
      there never followed a read or write call.
    - It does not to seem possible to block file access if a data management
      application that implements the Fanotify API has not been started. Events
      only seem to be generated if the data management application is in
      operation. Especially this limitation does not allow to use this
      technology.

    For DMAPI there has been implemented a reference implementation. One
    limitation is that it is only available for teh XFS file system type. Since
    only the SLES distributions fully provide this API it is not considered
    to fully support that.

    Fuse is an API to implement a file system in user space. By using this
    third approach it is possible to write an overlay file system on top
    on the original file system that should be managed. Read, write, or
    truncate system call can be intercepted within the overlay file system
    to let data be transferred back to disk. Other system calls can forwarded
    to the original file system. See @subpage fuse_connector for detailed
    information. Even this third possibility has disadvantages:

    - The original file system never should be directly accessed. All access
      should happen through the Fuse overlay file system.
    - Migrated files appear as empty file within the original file system.
      There is no practical way to make it impossible for a user to mount
      the original file system and therefore there is still a possibility
      to work with that empty files within the original file system.
    - Currently only POSIX functionality is provided by the Fuse overlay
      file system. Any file system type specific functionality is not
      available.

    it is the only practical technology that in contrast to DMAPI is
    fully developed for LTFS Data Management.

    ## The Connector

    The Connector is an API that should cover the very different technologies.
    Currently it is implemented for the Fuse overlay approach and the DMAPI
    reference implementation. If there will be a better or more suitable API
    for doing data management it should be possible to integrate that.

 */

struct fuid_t
{
    unsigned long fsid_h;
    unsigned long fsid_l;
    unsigned int igen;
    unsigned long inum;
    friend bool operator<(const fuid_t fuid1, const fuid_t fuid2)
    {
        return (fuid1.inum < fuid2.inum)
                || ((fuid1.inum == fuid2.inum)
                        && ((fuid1.igen < fuid2.igen)
                                || ((fuid1.igen == fuid2.igen)
                                        && ((fuid1.fsid_l < fuid2.fsid_l)
                                                || ((fuid1.fsid_l
                                                        == fuid2.fsid_l)
                                                        && ((fuid1.fsid_h
                                                                < fuid2.fsid_h)))))));
    }

    friend bool operator==(const fuid_t fuid1, const fuid_t fuid2)
    {
        return (fuid1.inum == fuid2.inum) && (fuid1.igen == fuid2.igen)
                && (fuid1.fsid_l == fuid2.fsid_l)
                && (fuid1.fsid_h == fuid2.fsid_h);
    }
    friend bool operator!=(const fuid_t fuid1, const fuid_t fuid2)
    {
        return !(fuid1 == fuid2);
    }
};

class Connector
{
private:
    struct timespec starttime;
    bool cleanup;
public:
    struct rec_info_t
    {
        struct conn_info_t *conn_info;
        bool toresident;
        fuid_t fuid;
        std::string filename;
    };
    static std::atomic<bool> connectorTerminate;
    static std::atomic<bool> forcedTerminate;
    static std::atomic<bool> recallEventSystemStopped;
    static Configuration *conf;

    Connector(bool _cleanup, Configuration *_conf = nullptr);
    ~Connector();
    struct timespec getStartTime()
    {
        return starttime;
    }
    void initTransRecalls();
    void endTransRecalls();
    rec_info_t getEvents();
    static void respondRecallEvent(rec_info_t recinfo, bool success);
    void terminate();
};

class FsObj
{
private:
    void *handle;
    unsigned long handleLength;
    bool isLocked;
    bool handleFree;
public:
    struct fs_attr_t
    {
        bool managed;
    };
    struct mig_attr_t
    {
        unsigned long typeId;
        bool added;
        int copies;
        char tapeId[Const::maxReplica][Const::tapeIdLength + 1];
    };
    enum file_state
    {
        RESIDENT,  /**< 0 */
        PREMIGRATED,  /**< 1 */
        MIGRATED,  /**< 2 */
        FAILED,  /**< 3 */
        PREMIGRATING,  /**< 4 */
        STUBBING,  /**< 5 */
        RECALLING_MIG,  /**< 6 */
        RECALLING_PREMIG  /**< 7 */
    };
    static std::string migStateStr(file_state migstate)
    {
        switch (migstate) {
            case RESIDENT:
                return messages[LTFSDMX0012I];
            case PREMIGRATED:
                return messages[LTFSDMX0011I];
            case MIGRATED:
                return messages[LTFSDMX0010I];
            case FAILED:
                return messages[LTFSDMX0019I];
            case PREMIGRATING:
                return messages[LTFSDMX0026I];
            case STUBBING:
                return messages[LTFSDMX0027I];
            case RECALLING_MIG:
            case RECALLING_PREMIG:
                return messages[LTFSDMX0028I];
            default:
                return "";
        }
    }
    FsObj(void *_handle, unsigned long _handleLength) :
            handle(_handle), handleLength(_handleLength),
            isLocked(false), handleFree(false)
    {
    }
    FsObj(std::string fileName);
    FsObj(Connector::rec_info_t recinfo);
    ~FsObj();
    bool isFsManaged();
    void manageFs(bool setDispo, struct timespec starttime);
    struct stat stat();
    fuid_t getfuid();
    std::string getTapeId();
    void lock();
    bool try_lock();
    void unlock();
    long read(long offset, unsigned long size, char *buffer);
    long write(long offset, unsigned long size, char *buffer);
    void addAttribute(mig_attr_t value);
    void remAttribute();
    mig_attr_t getAttribute();
    void preparePremigration();
    void finishPremigration();
    void prepareRecall();
    void finishRecall(file_state fstate);
    void prepareStubbing();
    void stub();
    file_state getMigState();
};
