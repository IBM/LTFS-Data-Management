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
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/resource.h>
#include <errno.h>

#include <string>
#include <sstream>
#include <exception>

#include "ltfsdm.pb.h"
#include "src/common/errors.h"
#include "src/common/LTFSDMException.h"
#include "src/common/Trace.h"

#include "LTFSDmComm.h"

std::atomic<bool> exitClient(false);

void LTFSDmCommClient::connect()

{
    struct sockaddr_un addr;

    if ((socRefFd = socket(AF_UNIX, SOCK_STREAM, 0)) == -1) {
        TRACE(Trace::error, errno);
        THROW(Error::GENERAL_ERROR);
    }

    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, sockFile.c_str(), sizeof(addr.sun_path) - 1);

    if (::connect(socRefFd, (struct sockaddr*) &addr, sizeof(addr)) == -1) {
        TRACE(Trace::error, errno);
        close(socRefFd);
        socRefFd = Const::UNSET;
        THROW(Error::GENERAL_ERROR);
    }

    GOOGLE_PROTOBUF_VERIFY_VERSION;
}

void LTFSDmCommServer::listen()

{
    struct sockaddr_un addr;

    if ((socRefFd = socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0)) == -1) {
        TRACE(Trace::error, errno);
        THROW(Error::GENERAL_ERROR);
    }

    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, sockFile.c_str(), sizeof(addr.sun_path) - 1);

    if (bind((int) socRefFd, (struct sockaddr*) &addr, sizeof(addr)) == -1) {
        TRACE(Trace::error, errno);
        ::close(socRefFd);
        socRefFd = Const::UNSET;
        THROW(Error::GENERAL_ERROR);
    }

    if (::listen(socRefFd, SOMAXCONN) == -1) {
        TRACE(Trace::error, errno);
        ::close(socRefFd);
        socRefFd = Const::UNSET;
        THROW(Error::GENERAL_ERROR);
    }
}

void LTFSDmCommServer::accept()

{
    if ((socAccFd = ::accept(socRefFd, NULL, NULL)) == -1) {
        TRACE(Trace::error, errno);
        socRefFd = Const::UNSET;
        THROW(Error::GENERAL_ERROR);
    }
}

void LTFSDmComm::send(int fd)

{
    unsigned long MessageSize;
    unsigned long rsize;
    char *buffer;

    if (exitClient) {
        MessageSize = 0;
        buffer = (char *) malloc(sizeof(long));
        memset(buffer, 0, sizeof(long));
    } else {
        MessageSize = this->ByteSizeLong();

        buffer = (char *) malloc(MessageSize + sizeof(long));
        memset(buffer, 0, MessageSize + sizeof(long));
        memcpy(buffer, &MessageSize, sizeof(long));
        if (this->SerializeToArray(buffer + sizeof(long), MessageSize) == false) {
            TRACE(Trace::error, buffer);
            THROW(Error::GENERAL_ERROR);
        }
    }

    TRACE(Trace::full, strlen(buffer), MessageSize);

    rsize = write(fd, buffer, MessageSize + sizeof(long));

    if (rsize != 0 && rsize != MessageSize + sizeof(long)) {
        free(buffer);
        TRACE(Trace::error, rsize, MessageSize, errno, sizeof(long));
        MSG(LTFSDMX0008E);
        THROW(Error::GENERAL_ERROR);
    }

    if (exitClient) {
        THROW(Error::GENERAL_ERROR);
    }

    free(buffer);
}

ssize_t readx(int fd, char *buffer, size_t size)

{
    unsigned long bread = 0;
    ssize_t rsize;
    while (bread < size) {
        rsize = read(fd, buffer + bread, size - bread);
        if (rsize == 0) {
            break;
        } else if (rsize == -1) {
            TRACE(Trace::error, errno);
            return -1;
        }
        bread += rsize;
    }

    return bread;
}

void LTFSDmComm::recv(int fd)

{
    ssize_t MessageSize;
    ssize_t rsize;
    char *buffer;

    rsize = readx(fd, (char *) &MessageSize, sizeof(long));

    if (rsize != sizeof(long)) {
        TRACE(Trace::error, rsize, sizeof(long));
        THROW(Error::GENERAL_ERROR);
    }

    if (MessageSize == 0)
        THROW(Error::GENERAL_ERROR);

    TRACE(Trace::full, MessageSize);

    buffer = (char *) malloc(MessageSize);
    memset(buffer, 0, MessageSize);

    rsize = readx(fd, buffer, MessageSize);

    if (rsize != MessageSize) {
        TRACE(Trace::error, rsize, MessageSize);
        free(buffer);
        THROW(Error::GENERAL_ERROR);
    }

    if (rsize == 0) {
        THROW(Error::GENERAL_ERROR);
    }

    this->ParseFromArray(buffer, MessageSize);
    free(buffer);
}
