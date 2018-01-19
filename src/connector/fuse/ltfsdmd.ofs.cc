#include <unistd.h>
#include <limits.h>
#include <sys/types.h>
#include <dirent.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/ioctl.h>
#include <blkid/blkid.h>
#include <uuid/uuid.h>

#include <string>
#include <sstream>
#include <thread>
#include <vector>
#include <map>
#include <set>

#include "src/common/errors/errors.h"
#include "src/common/exception/LTFSDMException.h"
#include "src/common/util/util.h"
#include "src/common/util/FileSystems.h"
#include "src/common/messages/Message.h"
#include "src/common/tracing/Trace.h"
#include "src/common/const/Const.h"
#include "src/common/configuration/Configuration.h"

#include "src/common/comm/ltfsdm.pb.h"
#include "src/common/comm/LTFSDmComm.h"

#include "src/connector/Connector.h"
#include "src/connector/fuse/FuseLock.h"
#include "src/connector/fuse/FuseFS.h"

void getUUID(std::string fsName, uuid_t *uuid)

{
    blkid_cache cache;
    char *uuidstr;
    int rc;

    if ((rc = blkid_get_cache(&cache, NULL)) != 0) {
        TRACE(Trace::error, rc, errno);
        MSG(LTFSDMF0055E);
        THROW(Error::GENERAL_ERROR, errno);
    }

    if ((uuidstr = blkid_get_tag_value(cache, "UUID", fsName.c_str())) == NULL) {
        blkid_put_cache(cache);
        TRACE(Trace::error, errno);
        MSG(LTFSDMF0055E);
        THROW(Error::GENERAL_ERROR, errno);
    }

    if ((rc = uuid_parse(uuidstr, *uuid)) != 0) {
        blkid_put_cache(cache);
        free(uuidstr);
        TRACE(Trace::error, rc, errno);
        MSG(LTFSDMF0055E);
        THROW(Error::GENERAL_ERROR, errno);
    }

    blkid_put_cache(cache);
    free(uuidstr);
}

/**
    @file
    @brief File that provides the main entry point for the Fuse overlay file system.
    @details
    For every file system that is managed with LTFS Data Management an Fuse
    overlay file system is created as a separate process. This file provides
    main entry point that also acts as an interface to the LTFS Data Management
    backend. Within the main entry point the following things are done:

    - An option processing is performed to receive information that is provided
      by the backend.
    - Messaging and Tracing is setup.
    - The 128bit file system uuid is determined.
    - The Fuse options are set.
    - The Fuse shared information is set.

 */

/**
    @param argc
    @param argv
    @return
 */
int main(int argc, char **argv)

{
    std::string mountpt("");
    std::string fsName("");
    struct timespec starttime = { 0, 0 };
    pid_t mainpid;
    uuid_t uuid;
    Message::LogType logType;
    Trace::traceLevel tl;
    bool logTypeSet = false;
    bool traceLevelSet = false;
    int opt;
    opterr = 0;

    const struct fuse_operations ltfsdm_operations = FuseFS::init_operations();
    struct fuse_args fargs;
    std::stringstream options;

    while ((opt = getopt(argc, argv, "m:f:S:N:l:t:p:")) != -1) {
        switch (opt) {
            case 'm':
                if (mountpt.compare("") != 0)
                    return static_cast<int>(Error::GENERAL_ERROR);
                mountpt = optarg;
                break;
            case 'f':
                if (fsName.compare("") != 0)
                    return static_cast<int>(Error::GENERAL_ERROR);
                fsName = optarg;
                break;
            case 'S':
                if (starttime.tv_sec != 0)
                    return static_cast<int>(Error::GENERAL_ERROR);
                starttime.tv_sec = std::stol(optarg, nullptr);
                break;
            case 'N':
                if (starttime.tv_nsec != 0)
                    return static_cast<int>(Error::GENERAL_ERROR);
                starttime.tv_nsec = std::stol(optarg, nullptr);
                break;
            case 'l':
                if (logTypeSet)
                    return static_cast<int>(Error::GENERAL_ERROR);
                logType = static_cast<Message::LogType>(std::stoi(optarg,
                        nullptr));
                logTypeSet = true;
                break;
            case 't':
                if (traceLevelSet)
                    return static_cast<int>(Error::GENERAL_ERROR);
                tl = static_cast<Trace::traceLevel>(std::stoi(optarg, nullptr));
                traceLevelSet = true;
                break;
            case 'p':
                if (mainpid)
                    return static_cast<int>(Error::GENERAL_ERROR);
                mainpid = static_cast<pid_t>(std::stoi(optarg, nullptr));
                break;
            default:
                return static_cast<int>(Error::GENERAL_ERROR);
        }
    }

    if (optind != 15) {
        MSG(LTFSDMF0004E);
        return static_cast<int>(Error::GENERAL_ERROR);
    }

    std::string mp = mountpt;
    std::replace(mp.begin(), mp.end(), '/', '.');
    messageObject.init(mp);
    messageObject.setLogType(logType);

    MSG(LTFSDMF0031I);

    try {
        traceObject.init(mp);
        traceObject.setTrclevel(tl);
    } catch (const std::exception& e) {
        exit((int) Error::GENERAL_ERROR);
    }

    try {
        getUUID(fsName, &uuid);
    } catch (const std::exception& e) {
        exit((int) Error::GENERAL_ERROR);
    }

    MSG(LTFSDMF0001I, mountpt + Const::LTFSDM_CACHE_MP, mountpt);

    fargs = FUSE_ARGS_INIT(0, NULL);
    fuse_opt_add_arg(&fargs, argv[0]);
    fuse_opt_add_arg(&fargs, mountpt.c_str());
    options << "-ouse_ino,fsname=LTFSDM:" << fsName
//            << ",nopath,default_permissions,allow_other,kernel_cache,hard_remove,max_background="
            << ",nopath,default_permissions,allow_other,kernel_cache,max_background="
            << Const::MAX_FUSE_BACKGROUND;
    fuse_opt_add_arg(&fargs, options.str().c_str());
    fuse_opt_add_arg(&fargs, "-f");
    if (getppid() != 1 && traceObject.getTrclevel() == Trace::full)
        fuse_opt_add_arg(&fargs, "-d");

    MSG(LTFSDMF0002I, mountpt.c_str());

    FuseFS::shared_data sd {
        Const::UNSET,
        mountpt,
        starttime,
        LTFSDM::getkey(),
        be64toh(*(unsigned long *) &uuid[0]),
        be64toh(*(unsigned long *) &uuid[8]),
        mainpid,
        mountpt + Const::LTFSDM_CACHE_MP
    };

    return fuse_main(fargs.argc, fargs.argv, &ltfsdm_operations, (void * ) &sd);
}
