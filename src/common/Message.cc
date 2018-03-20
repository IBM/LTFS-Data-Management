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
#include <sstream>
#include <vector>
#include <set>
#include <mutex>

#include "src/common/Const.h"
#include "src/common/errors.h"
#include "src/common/util.h"
#include "src/common/LTFSDMException.h"

#include "Message.h"

Message messageObject;

Message::~Message()

{
    if (fd != Const::UNSET)
        close(fd);

    fd = Const::UNSET;
}

void Message::init(std::string extension)

{
    if (extension.compare("") != 0)
        fileName.append(extension);

    fd = open(fileName.c_str(),
    O_RDWR | O_CREAT | O_APPEND | O_CLOEXEC | O_SYNC, 0644);

    if (fd == Const::UNSET) {
        MSG(LTFSDMX0003E, errno);
        THROW(Error::GENERAL_ERROR, errno);
    }
}

void Message::writeOut(std::string msgstr)

{
    mtx.lock();
    std::cout << msgstr << std::flush;
    mtx.unlock();
}

void Message::writeLog(std::string msgstr)

{
    if (write(fd, msgstr.c_str(), msgstr.size()) != (long) msgstr.size()) {
        std::cerr << ltfsdm_messages[LTFSDMX0004E];
        exit((int) Error::GENERAL_ERROR);
    }
}
