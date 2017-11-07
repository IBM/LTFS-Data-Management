#include <sys/resource.h>
#include <libmount/libmount.h>
#include <blkid/blkid.h>
#include <errno.h>

#include <string>
#include <sstream>
#include <vector>

#include "src/common/errors/errors.h"
#include "src/common/exception/OpenLTFSException.h"
#include "src/common/messages/Message.h"
#include "src/common/tracing/Trace.h"
#include "src/common/const/Const.h"

#include "FileSystems.h"

FileSystems::FileSystems() :
        first(true), tb(NULL)

{
    int rc;

    cxt = mnt_new_context();

    if ((rc = blkid_get_cache(&cache, NULL)) != 0) {
        TRACE(Trace::error, rc, errno);
        THROW(Error::GENERAL_ERROR, rc, errno);
    }
}

FileSystems::~FileSystems()

{
    blkid_put_cache(cache);
    mnt_free_context(cxt);
}

void FileSystems::getTable()

{
    int rc;

    if ((rc = mnt_context_get_mtab(cxt, &tb)) != 0) {
        blkid_put_cache(cache);
        TRACE(Trace::error, rc, errno);
        THROW(Error::GENERAL_ERROR, rc, errno);
    }
}

FileSystems::fsinfo FileSystems::getContext(struct libmnt_fs *mntfs)

{
    FileSystems::fsinfo fs;
    const char *str;
    char *uuid;

    if ((str = mnt_fs_get_source(mntfs)) == NULL)
        THROW(Error::GENERAL_ERROR);
    else
        fs.source = str;

    if ((str = mnt_fs_get_target(mntfs)) == NULL)
        THROW(Error::GENERAL_ERROR);
    else
        fs.target = str;

    if ((str = mnt_fs_get_fstype(mntfs)) == NULL)
        THROW(Error::GENERAL_ERROR);
    else
        fs.fstype = str;

    if ((str = mnt_fs_get_options(mntfs)) == NULL)
        THROW(Error::GENERAL_ERROR);
    else
        fs.options = str;

    if ((uuid = blkid_get_tag_value(cache, "UUID", fs.source.c_str())) == NULL) {
        fs.uuid = "";
    } else {
        fs.uuid = uuid;
        free(uuid);
    }

    return fs;
}

std::vector<FileSystems::fsinfo> FileSystems::getAll()

{
    std::vector<FileSystems::fsinfo> fslist;
    fsinfo fs;
    struct libmnt_fs *mntfs;
    struct libmnt_iter *itr;

    getTable();

    if ((itr = mnt_new_iter(MNT_ITER_BACKWARD)) == NULL) {
        TRACE(Trace::error, errno);
        THROW(Error::GENERAL_ERROR, errno);
    }

    while (mnt_table_next_fs(tb, itr, &mntfs) == 0) {
        try {
            fs = getContext(mntfs);
        } catch (const std::exception& e) {
            mnt_free_iter(itr);
            TRACE(Trace::error, e.what());
            THROW(Error::GENERAL_ERROR);
        }

        if (fs.fstype.compare("xfs") == 0 || fs.fstype.compare("ext3") == 0
                || fs.fstype.compare("ext4") == 0) {
            TRACE(Trace::always, fs.source, fs.target, fs.fstype, fs.options,
                    fs.uuid);
            fslist.push_back(fs);
        }
    }

    mnt_free_iter(itr);

    return fslist;
}

FileSystems::fsinfo FileSystems::getByTarget(std::string target)

{
    struct libmnt_fs *mntfs;
    fsinfo fs;

    getTable();

    if ((mntfs = mnt_table_find_target(tb, target.c_str(), MNT_ITER_BACKWARD))
            == NULL) {
        TRACE(Trace::error, target);
        THROW(Error::GENERAL_ERROR, target);
    }

    try {
        fs = getContext(mntfs);
    } catch (const std::exception& e) {
        TRACE(Trace::error, e.what());
        THROW(Error::GENERAL_ERROR, target);
    }

    return fs;
}

void FileSystems::mount(std::string source, std::string target,
        std::string options)

{
    int rc;

    if ((rc = mnt_reset_context(cxt)) != 0) {
        TRACE(Trace::error, target, rc);
        THROW(Error::GENERAL_ERROR, target, rc);
    }

    getTable();

    if ((rc = mnt_context_set_source(cxt, source.c_str())) != 0) {
        TRACE(Trace::error, target, rc);
        THROW(Error::GENERAL_ERROR, target, rc);
    }

    if ((rc = mnt_context_set_target(cxt, target.c_str())) != 0) {
        TRACE(Trace::error, target, rc);
        THROW(Error::GENERAL_ERROR, target, rc);
    }

    if ((rc = mnt_context_set_options(cxt, options.c_str())) != 0) {
        TRACE(Trace::error, target, rc);
        THROW(Error::GENERAL_ERROR, target, rc);
    }

    if (mnt_context_mount(cxt) == 0)
        return;

    if ((rc = mnt_context_get_status(cxt)) != 1) {
        TRACE(Trace::error, target, rc, mnt_context_get_syscall_errno(cxt));
        THROW(Error::GENERAL_ERROR, target, rc);
    }
}

void FileSystems::umount(std::string target, umountflag flag)

{
    struct libmnt_fs *mntfs;
    fsinfo fs;
    int rc;
    int err;

    if ((rc = mnt_reset_context(cxt)) != 0) {
        TRACE(Trace::error, target, rc);
        THROW(Error::GENERAL_ERROR, target, rc);
    }

    getTable();

    if ((mntfs = mnt_table_find_target(tb, target.c_str(), MNT_ITER_BACKWARD))
            == NULL) {
        TRACE(Trace::error, target);
        THROW(Error::GENERAL_ERROR, target);
    }

    if (first == false) {
        if ((rc = mnt_reset_context(cxt)) != 0) {
            TRACE(Trace::error, target);
            THROW(Error::GENERAL_ERROR, target);
        }
    } else {
        first = true;
    }

    if (flag == FileSystems::UMNT_DETACHED
            || flag == FileSystems::UMNT_DETACHED_FORCED) {
        if ((rc = mnt_context_enable_lazy(cxt, TRUE)) != 0) {
            TRACE(Trace::error, target, rc);
            THROW(Error::GENERAL_ERROR, target, rc);
        }
    }

    if (flag == FileSystems::UMNT_FORCED
            || flag == FileSystems::UMNT_DETACHED_FORCED) {
        if ((rc = mnt_context_enable_force(cxt, TRUE)) != 0) {
            TRACE(Trace::error, target, rc);
            THROW(Error::GENERAL_ERROR, target, rc);
        }
    }

    if ((rc = mnt_context_set_fs(cxt, mntfs)) != 0) {
        TRACE(Trace::error, target, rc);
        THROW(Error::GENERAL_ERROR, target, rc);
    }

    if (mnt_context_umount(cxt) == 0)
        return;

    if ((rc = mnt_context_get_status(cxt)) != 1) {
        err = mnt_context_get_syscall_errno(cxt);
        TRACE(Trace::error, target, rc, err);
        if (err == EBUSY)
            THROW(Error::FS_BUSY, target, rc);
        else
            THROW(Error::GENERAL_ERROR, target, rc);
    }
}
