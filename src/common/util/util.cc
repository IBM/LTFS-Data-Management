#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <dirent.h>
#include <mntent.h>
#include <sys/resource.h>
#include <errno.h>

#include <string>
#include <fstream>
#include <set>
#include <vector>
#include <sstream>
#include <exception>

#include "src/common/errors/errors.h"
#include "src/common/exception/OpenLTFSException.h"
#include "src/common/messages/Message.h"
#include "src/common/tracing/Trace.h"
#include "src/common/const/Const.h"

#include "util.h"

void mkTmpDir()

{
    struct stat statbuf;

    if (stat(Const::LTFSDM_TMP_DIR.c_str(), &statbuf) != 0) {
        if (mkdir(Const::LTFSDM_TMP_DIR.c_str(), 0700) != 0) {
            std::cerr << messages[LTFSDMX0006E] << Const::LTFSDM_TMP_DIR
                    << std::endl;
            THROW(Error::GENERAL_ERROR);
        }
    } else if (!S_ISDIR(statbuf.st_mode)) {
        std::cerr << Const::LTFSDM_TMP_DIR << messages[LTFSDMX0007E]
                << std::endl;
        THROW(Error::GENERAL_ERROR);
    }
}

//! [init]
void LTFSDM::init(std::string ident)

{
    mkTmpDir();
    messageObject.init(ident);
    traceObject.init(ident);
}
//! [init]

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
        THROW(Error::GENERAL_ERROR);
    }

    keyFile.close();

    return key;
}
