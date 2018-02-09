/*******************************************************************************
 * Copyright 2018 IBM Corp. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *  https://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 *******************************************************************************/
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
#include "src/common/exception/LTFSDMException.h"
#include "src/common/messages/Message.h"
#include "src/common/tracing/Trace.h"
#include "src/common/const/Const.h"

#include "util.h"

void mkTmpDir()

{
    struct stat statbuf;

    if (stat(Const::LTFSDM_TMP_DIR.c_str(), &statbuf) != 0) {
        if (mkdir(Const::LTFSDM_TMP_DIR.c_str(), 0700) != 0) {
            std::cerr << ltfsdm_messages[LTFSDMX0006E] << Const::LTFSDM_TMP_DIR
                    << std::endl;
            THROW(Error::GENERAL_ERROR);
        }
    } else if (!S_ISDIR(statbuf.st_mode)) {
        std::cerr << Const::LTFSDM_TMP_DIR << ltfsdm_messages[LTFSDMX0007E]
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

int LTFSDM::stat_retry(const char *pathname, struct stat *buf)

{
    int rc;
    int retry = 0;

    while ((rc = stat(pathname, buf)) == -1 && errno == EBUSY && retry < 10) {
        TRACE(Trace::error, pathname);
        retry++;
        sleep(1);
    }

    return rc;
}

int LTFSDM::open_retry(const char *pathname, int flags)

{
    int rc;
    int retry = 0;

    while ((rc = open(pathname, flags)) == -1 && errno == EBUSY && retry < 10) {
        TRACE(Trace::error, pathname);
        retry++;
        sleep(1);
    }

    return rc;
}

int LTFSDM::open_retry(const char *pathname, int flags, mode_t mode)

{
    int rc;
    int retry = 0;

    while ((rc = open(pathname, flags, mode)) == -1 && errno == EBUSY && retry < 10) {
        TRACE(Trace::error, pathname);
        retry++;
        sleep(1);
    }

    return rc;
}

