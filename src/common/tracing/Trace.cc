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
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/types.h>
#include <signal.h>
#include <sys/resource.h>

#include <iostream>
#include <fstream>
#include <set>
#include <mutex>

#include "src/common/const/Const.h"
#include "src/common/messages/Message.h"
#include "src/common/errors/errors.h"
#include "src/common/util/util.h"
#include "src/common/exception/LTFSDMException.h"

#include "Trace.h"

extern const char *__progname;

Trace traceObject;

Trace::~Trace()

{
    if (fd != Const::UNSET)
        close(fd);
    fd = Const::UNSET;
}

void Trace::setTrclevel(traceLevel level)

{
    traceLevel oldLevel;

    switch (level) {
        case Trace::none:
            trclevel = level;
            break;
        case Trace::error:
        case Trace::normal:
        case Trace::full:
            oldLevel = trclevel;
            TRACE(Trace::error, oldLevel, level);
            trclevel = level;
            break;
        default:
            trclevel = Trace::error;
            break;
    }
}

int Trace::getTrclevel()

{
    return trclevel;
}

void Trace::init(std::string extension)

{
    if (extension.compare("") != 0)
        fileName.append(extension);

    fd = open(fileName.c_str(),
    O_RDWR | O_CREAT | O_APPEND | O_CLOEXEC | O_SYNC, 0644);

    if (fd == Const::UNSET) {
        MSG(LTFSDMX0001E, errno);
        THROW(Error::GENERAL_ERROR, errno);
    }
}

void Trace::rotate()

{
    if (lseek(fd, 0, SEEK_CUR) < 100 * 1024 * 1024)
        return;

    close(fd);
    fd = Const::UNSET;

    if (unlink((fileName + ".2").c_str()) == -1 && errno != ENOENT) {
        MSG(LTFSDMX0031E, errno);
        THROW(Error::GENERAL_ERROR, errno);
    } else if (rename((fileName + ".1").c_str(), (fileName + ".2").c_str())
            == -1 && errno != ENOENT) {
        MSG(LTFSDMX0031E, errno);
        THROW(Error::GENERAL_ERROR, errno);
    } else if (rename(fileName.c_str(), (fileName + ".1").c_str())
            == -1&& errno != ENOENT) {
        MSG(LTFSDMX0031E, errno);
        THROW(Error::GENERAL_ERROR, errno);
    }

    init("");
}
