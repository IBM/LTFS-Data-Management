#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <dirent.h>
#include <mntent.h>
#include <sys/resource.h>

#include <string>
#include <fstream>
#include <set>
#include <vector>
#include <sstream>
#include <exception>

#include "src/common/exception/OpenLTFSException.h"
#include "src/common/messages/Message.h"
#include "src/common/tracing/Trace.h"
#include "src/common/errors/errors.h"
#include "src/common/const/Const.h"

#include "util.h"

void mkTmpDir()

{
    struct stat statbuf;

    if (stat(Const::LTFSDM_TMP_DIR.c_str(), &statbuf) != 0) {
        if (mkdir(Const::LTFSDM_TMP_DIR.c_str(), 0700) != 0) {
            std::cerr << messages[LTFSDMX0006E] << Const::LTFSDM_TMP_DIR
                    << std::endl;
            THROW(Const::UNSET);
        }
    } else if (!S_ISDIR(statbuf.st_mode)) {
        std::cerr << Const::LTFSDM_TMP_DIR << messages[LTFSDMX0007E]
                << std::endl;
        THROW(Const::UNSET);
    }
}

void LTFSDM::init(std::string ident)

{
    mkTmpDir();
    messageObject.init(ident);
    traceObject.init(ident);
}

std::vector<std::string> LTFSDM::getTapeIds()

{
    std::vector<std::string> tapeIds;
    std::ifstream tpfile("/tmp/tapeids");
    std::string line;

    while (std::getline(tpfile, line)) {
        tapeIds.push_back(line);
    }

    return tapeIds;
}

std::set<std::string> LTFSDM::getFs()

{
    unsigned int buflen = 32 * 1024;
    char *buffer = NULL;
    struct mntent mntbuf;
    FILE *MNTINFO;
    std::set<std::string> mountList;

    MNTINFO = setmntent("/proc/mounts", "r");
    if (!MNTINFO) {
        THROW(errno, errno);
    }

    buffer = (char *) malloc(buflen);

    if (buffer == NULL)
        THROW(Const::UNSET);

    while (getmntent_r(MNTINFO, &mntbuf, buffer, buflen))
        if (!strcmp(mntbuf.mnt_type, "xfs") || !strcmp(mntbuf.mnt_type, "ext4"))
            mountList.insert(std::string(mntbuf.mnt_dir));

    endmntent(MNTINFO);

    free(buffer);

    return mountList;
}

std::string LTFSDM::getDev(std::string mountpt)

{
    std::string devName = " - ";
    unsigned int buflen = 32 * 1024;
    char buffer[32 * 1024];
    struct mntent mntbuf;
    FILE *MNTINFO;

    MNTINFO = setmntent("/proc/mounts", "r");
    if (!MNTINFO) {
        THROW(errno, errno);
    }

    memset(buffer, 0, sizeof(buffer));

    if (buffer == NULL)
        THROW(Const::UNSET);

    while (getmntent_r(MNTINFO, &mntbuf, buffer, buflen)) {
        if (mountpt.compare(mntbuf.mnt_dir) == 0) {
            devName = std::string(mntbuf.mnt_fsname);
            break;
        }
    }
    endmntent(MNTINFO);

    return devName;
}

long LTFSDM::getkey()

{
    std::ifstream keyFile;
    std::string line;
    long key = Const::UNSET;

    keyFile.exceptions(std::ofstream::failbit | std::ofstream::badbit);

    try {
        keyFile.open(Const::KEY_FILE);
        std::getline(keyFile, line);
        key = std::stol(line);
    } catch (const std::exception& e) {
        TRACE(Trace::error, key);
        MSG(LTFSDMX0030E);
        THROW(Error::LTFSDM_GENERAL_ERROR);
    }

    keyFile.close();

    return key;
}
