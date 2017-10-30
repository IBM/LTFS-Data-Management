#include <sys/resource.h>
#include <libmount/libmount.h>
#include <blkid/blkid.h>
#include <errno.h>

#include <string>
#include <sstream>
#include <vector>

#include "src/common/exception/OpenLTFSException.h"
#include "src/common/messages/Message.h"
#include "src/common/tracing/Trace.h"
#include "src/common/errors/errors.h"
#include "src/common/const/Const.h"

#include "FileSystems.h"

std::vector<FileSystems::fsinfo_t> FileSystems::getAll()

{
    std::vector<FileSystems::fsinfo_t> fslist;
    fsinfo_t fs;
    struct libmnt_context *cxt = mnt_new_context();
    struct libmnt_iter *itr;
    struct libmnt_table *tb;
    struct libmnt_fs *mntfs;
    std::string fstype;
    blkid_cache cache;
    char *uuid;

    int rc;

    if ( (rc = blkid_get_cache(&cache, NULL)) != 0 ) {
        TRACE(Trace::error, rc, errno);
        THROW(errno, rc, errno);
    }

    if ( (rc = mnt_context_get_mtab(cxt, &tb)) != 0 ) {
        TRACE(Trace::error, rc, errno);
        THROW(errno, rc, errno);
    }

    if ( (itr = mnt_new_iter(MNT_ITER_BACKWARD)) == NULL ) {
        mnt_free_context(cxt);
        TRACE(Trace::error, errno);
        THROW(errno, errno);
    }

    while ( mnt_table_next_fs(tb, itr, &mntfs) == 0) {
        fstype = mnt_fs_get_fstype(mntfs);
        if ( fstype.compare("xfs") == 0
                || fstype.compare("ext3") == 0
                || fstype.compare("ext4") == 0 ) {
            fs.source = mnt_fs_get_source(mntfs);
            fs.target = mnt_fs_get_target(mntfs);
            fs.fstype = fstype;
            fs.options = mnt_fs_get_options(mntfs);
        }
        else {
            continue;
        }

        uuid = blkid_get_tag_value(cache, "UUID", fs.source.c_str());
        if ( uuid == NULL )
            continue;
        fs.uuid = uuid;

        TRACE(Trace::always, fs.source, fs.target, fs.fstype, fs.options, fs.uuid);

        fslist.push_back(fs);
    }

    blkid_put_cache(cache);

    mnt_free_iter(itr);
    mnt_free_context(cxt);

    return fslist;
}

FileSystems::fsinfo_t FileSystems::getByTarget(std::string target)

{
    for ( fsinfo_t& fs : FileSystems::getAll() ) {
        if ( fs.target.compare(target) == 0 ) {
            return fs;
        }
    }

    TRACE(Trace::error, target);
    THROW(Error::LTFSDM_GENERAL_ERROR, target);
}
