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
#include <sys/file.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <fcntl.h>
#include <errno.h>

#include <string>
#include <mutex>

#include "src/common/errors/errors.h"
#include "src/common/messages/Message.h"
#include "src/common/tracing/Trace.h"
#include "src/common/const/Const.h"

#include "src/connector/fuse/FuseLock.h"

std::mutex FuseLock::mtx;

FuseLock::FuseLock(std::string identifier, FuseLock::lockType _type,
        FuseLock::lockOperation _operation) :
        id(identifier), fd(Const::UNSET), type(_type), operation(_operation)

{
    id += '.';
    id += type;

    std::lock_guard<std::mutex> lock(FuseLock::mtx);

    if ((fd = open(id.c_str(), O_RDONLY | O_CLOEXEC)) == -1) {
        TRACE(Trace::error, id, errno);
        THROW(Error::GENERAL_ERROR, id, errno);
    }
}

FuseLock::~FuseLock()

{
    if (fd != Const::UNSET)
        close(fd);
}

void FuseLock::lock()

{
    if (flock(fd, (operation == FuseLock::lockshared ? LOCK_SH : LOCK_EX))
            == -1) {
        TRACE(Trace::error, id, errno);
        THROW(Error::GENERAL_ERROR, id, errno);
    }
}

bool FuseLock::try_lock()

{
    if (flock(fd,
            (operation == FuseLock::lockshared ? LOCK_SH : LOCK_EX) | LOCK_NB)
            == -1) {
        if ( errno == EWOULDBLOCK)
            return false;
        TRACE(Trace::error, id, errno);
        THROW(Error::GENERAL_ERROR, id, errno);
    }

    return true;
}

void FuseLock::unlock()

{
    if (flock(fd, LOCK_UN) == -1) {
        TRACE(Trace::error, id, errno);
        THROW(Error::GENERAL_ERROR, id, errno);
    }
}
