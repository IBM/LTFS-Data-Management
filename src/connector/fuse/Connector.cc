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
#include <linux/fs.h>
#include <dirent.h>
#include <sys/resource.h>
#include <libmount/libmount.h>
#include <blkid/blkid.h>

#include <sstream>
#include <map>
#include <set>
#include <vector>
#include <thread>

#include "src/common/errors.h"
#include "src/common/LTFSDMException.h"
#include "src/common/util.h"
#include "src/common/FileSystems.h"
#include "src/common/Message.h"
#include "src/common/Trace.h"
#include "src/common/Const.h"
#include "src/common/Configuration.h"

#include "src/communication/ltfsdm.pb.h"
#include "src/communication/LTFSDmComm.h"

#include "src/connector/fuse/FuseLock.h"
#include "src/connector/fuse/FuseFS.h"
#include "src/connector/fuse/FuseConnector.h"
#include "src/connector/Connector.h"

/** @page fuse_connector Fuse connector

    # Fuse Connector

    ## The Fuse overlay file system

    The Fuse Connector provides an overlay file system on top of an original
    file system that is managed by LTFS Data Management. File system access
    must only be performed by the Fuse overlay file system.

    The Fuse overlay file system has the following two basic tasks:

    - It provides some kind of control over the original file system.
      Most of the file system accesses are bypassed to the original.
      Read, write, and truncate system calls are suspended on
      migrated files to copy data from tape to disk.
    - If a file is migrated, within the original file system this
      file is truncated to zero size. The Fuse overlay file system
      still shows this file in its original size. This is necessary
      for applications to perform i/o operations in the same way as
      data is locally available (resident or premigrated state). Some
      LTFS Data Management attributes are hidden because these are
      only internally used. The access and modification time stamps
      should be kept.

    The following table shows how how a file appears in different migration
    states (resident, premigrated, migrated) within the two file systems. The
    Fuse overlay file system is virtual in a sense that there is no storage
    directly managed and all data and meta data are stored "somewhere else".
    Nevertheless it is possible to access data and meta data from the original
    file system.

    <table class="doxtable">
    <tr><th></th><th>resident/<BR>premigrated</th><th>migrated</th></tr>
    <tr>
    <td>Fuse overlay file system</td>
    <td>
        @dot digraph resident_1 {
            compound=true;
            fontname="courier";
            fontsize=11;
            labeljust=l;
            node [shape=record, fontname="courier", fontsize=11, fillcolor=white, style=filled];
            fuse [label="meta\ndata|------- data -------"];
        } @enddot
    </td>
    <td>
        @dot digraph migrated_1 {
            compound=true;
            fontname="courier";
            fontsize=11;
            labeljust=l;
            node [shape=record, fontname="courier", fontsize=11, fillcolor=white, style=filled];
            fuse [label="meta\ndata|------- data -------"];
        } @enddot
    </td>
    </tr>
    <tr>
    <td>original file system</td>
    <td>
        @dot digraph resident_2 {
            compound=true;
            fontname="courier";
            fontsize=11;
            labeljust=l;
            node [shape=record, fontname="courier", fontsize=11, fillcolor=white, style=filled];
            orig [label="meta\ndata|------- data -------"];
        } @enddot
    </td>
    <td>
        @dot digraph migrated_2 {
            compound=true;
            fontname="courier";
            fontsize=11;
            labeljust=l;
            node [shape=record, fontname="courier", fontsize=11, fillcolor=white, style=filled];
            orig [label="meta\ndata"];
        } @enddot
    </td>
    </tr>
    </table>

    Within the Fuse overlay file system the stat information of a migrated file
    looks as follows:

    @verbatim
      File: ‘file.1’
      Size: 6335488     Blocks: 8          IO Block: 4096   regular file
    Device: 42h/66d Inode: 3312825     Links: 1
    Access: (0644/-rw-r--r--)  Uid: (    0/    root)   Gid: (    0/    root)
    Access: 2018-01-12 14:57:16.780451059 +0100
    Modify: 2018-01-12 14:57:06.096451529 +0100
    Change: 2018-01-12 15:24:22.584379530 +0100
     Birth: -
    @endverbatim

    For the file the following is the stat information within the original
    file system:

    @verbatim
      File: ‘file.1’
      Size: 0           Blocks: 8          IO Block: 4096   regular empty file
    Device: 831h/2097d  Inode: 3312825     Links: 1
    Access: (0644/-rw-r--r--)  Uid: (    0/    root)   Gid: (    0/    root)
    Access: 2018-01-12 15:23:40.800381368 +0100
    Modify: 2018-01-12 15:24:22.584379530 +0100
    Change: 2018-01-12 15:24:22.584379530 +0100
     Birth: -
    @endverbatim

    For each file system that is managed a Fuse overlay file system is created.
    Each of these file systems is operating within a different process. If
    there are three file systems (/dev/loop0, /dev/loop1, and /dev/loop2)
    which are managed with LTFS Data Management it looks like the following:

    - mount information before managing:
    @verbatim
    vex:~ # cat /proc/mounts |grep loop
    /dev/loop0 /mnt/loop0 ext4 rw,relatime,data=ordered 0 0
    /dev/loop1 /mnt/loop1 ext4 rw,relatime,data=ordered 0 0
    /dev/loop2 /mnt/loop2 ext4 rw,relatime,data=ordered 0 0
    @endverbatim
    - now managing with LTFS Data Management:
    @verbatim
    vex:~ # ltfsdm add /mnt/loop0
    vex:~ # ltfsdm add /mnt/loop1
    vex:~ # ltfsdm add /mnt/loop2
    @endverbatim
    - process and mount information thereafter:
    @verbatim
    vex:~ # cat /proc/mounts |grep loop
    LTFSDM:/dev/loop0 /mnt/loop0 fuse rw,nosuid,nodev,relatime,user_id=0,group_id=0,default_permissions,allow_other 0 0
    LTFSDM:/dev/loop1 /mnt/loop1 fuse rw,nosuid,nodev,relatime,user_id=0,group_id=0,default_permissions,allow_other 0 0
    LTFSDM:/dev/loop2 /mnt/loop2 fuse rw,nosuid,nodev,relatime,user_id=0,group_id=0,default_permissions,allow_other 0 0

    vex:~ # ps -fp $(pidof ltfsdmd) $(pidof ltfsdmd.ofs)
    UID          PID    PPID  C STIME TTY      STAT   TIME CMD
    root     3987134       1  0 14:13 ?        Ssl    0:00 /root/OpenLTFS/bin/ltfsdmd
    root     3987592 3987591  0 14:18 ?        Sl     0:00 /root/OpenLTFS/bin/ltfsdmd.ofs -m /mnt/loop0 -f /dev/loop0 -S 1516194824 -N 335150020 -l 1 -t 2 -p 3987134
    root     3987610 3987609  0 14:18 ?        Sl     0:00 /root/OpenLTFS/bin/ltfsdmd.ofs -m /mnt/loop1 -f /dev/loop1 -S 1516194824 -N 335150020 -l 1 -t 2 -p 3987134
    root     3987625 3987624  0 14:18 ?        Sl     0:00 /root/OpenLTFS/bin/ltfsdmd.ofs -m /mnt/loop2 -f /dev/loop2 -S 1516194824 -N 335150020 -l 1 -t 2 -p 3987134
    @endverbatim

    Each of the ltfsdmd.ofs processes represent a different Fuse overlay file
    system.

    ## Startup

    File systems are managed by LTFS Data Management in two cases:

    - If a file system is added using the ltfsdm add command.
    - If a file system has been added previously: during the startup phase
      of LTFS Data management done automatically.

    The following two methods are involved when managing a file system:
    @code
    FsObj::manageFs
        -> FuseFS::init
    @endcode

    The FuseFS::init method works as documented:

    <BLOCKQUOTE>
    @copydoc FuseFS::init
    </BLOCKQUOTE>

    ## Code overview

    code part | description
    ---|---
    class Connector | @copybrief Connector
    class FsObj | @copybrief FsObj
    file ltfsdmd.ofs.cc | @copybrief ltfsdmd.ofs.cc
    class FuseFS | @copybrief FuseFS
    namespace FuseConnector | @copybrief FuseConnector

    ## Connector

    @copydetails Connector

    ## FsObj

    @copydetails FsObj

    ## ltfsdmd.ofs.cc

    @copydetails ltfsdmd.ofs.cc

    ## FuseFS

    @copydetails FuseFS

    ## Communication between LTFS Data Management backend and the Fuse overlay file system

    Communication between the LTFS Data Management backend and the Fuse
    overlay file system is performed in two different ways:

    - using the file system API
    - using a local socket

    The later one only is used to communicate transparent recall events and
    the correspondig responses. For the socket communication Google Protocol
    Buffers are used in the same way as the client talks to the backend.

    ### File system API communication

    In general the POSIX file operation calls are forwarded to the underlying
    original file system. E.g. if an attribute of a file within the Fuse
    overlay file system is changed, the attribute change actually is done
    at the corresponding file within the underlying original file system.
    However it is possible to transfer information that is not related
    to specific files, file content, or file attributes within the original
    file system that is managed. E.g. the overlay file system provides
    information of a virtual file that does not exist in the original file
    system. For normal files read or write calls are transferred to the
    original file within the original file system. The virtual file just
    is used for communication. The same can be done for virtual attributes
    that does not exist on files within the original file system. Using
    virtual attributes is a common way of communicating.

    One virtual attribute is also used by LTFS Data Management:
    - Const::LTFSDM_EA_FSINFO

    This virtual attribute provides information part of FuseFS::FuseHandle.

    A more appropriate way to communicate with the file system is the
    implementation of the ioctl call. This call is generally used to
    communicate with the kernel - also outside Fuse. The ioctl calls
    that are implemented here are the following:

    @snippet FuseFS.h ioctls

    ### Local socket communication

    For transparent recalls the Fuse overlay file system needs to inform
    the backend about recall events. The backend needs to provide the
    response if it finished successfully or not. For that purpose
    local socket communication is used since it already has been used
    for the communication between clients and the backend.

    The following Protocol Buffers messages are used for that purpose:

    - LTFSDmProtocol::LTFSDmTransRecRequest
    - LTFSDmProtocol::LTFSDmTransRecResp

    ### Mandatory file locking

    LTFS Data Management requires mandatory file locking. If a file data is
    just being transferred to tape applications should not be able to write to
    that file.

    To provides this functionality one possibility would be to to use the
    standard advisory locking and lock certain portions of that file to
    indicate that it is locked mandatorily. A problem with this approach is
    that such a lock can interfere with a similar lock performed by an
    application on the same file.

    The approach in LTFS Data Management is to use a virtual files for the
    purpose of locking. These files are not accessible from user space
    applications and have the following format:

    @par
    Const::LTFSDM_LOCK_DIR/@<inode number@>.[m|f]

    where 'm' and 'f' indicate different levels of locking.
 */

std::atomic<bool> Connector::connectorTerminate(false);
std::atomic<bool> Connector::forcedTerminate(false);
std::atomic<bool> Connector::recallEventSystemStopped(false);
Configuration *Connector::conf = nullptr;

LTFSDmCommServer recrequest(Const::RECALL_SOCKET_FILE);

Connector::Connector(bool _cleanup, Configuration *_conf) :
        cleanup(_cleanup)

{
    clock_gettime(CLOCK_REALTIME, &starttime);
    FuseConnector::ltfsdmKey = LTFSDM::getkey();
    conf = _conf;
}

Connector::~Connector()

{
    if (cleanup) {
        try {
            std::string mountpt;

            MSG(LTFSDMF0025I);
            FuseConnector::managedFss.clear();
            MSG(LTFSDMF0027I);
        } catch (const std::exception& e) {
            TRACE(Trace::error, e.what());
            kill(getpid(), SIGTERM);
        }
    }
}

void Connector::initTransRecalls()

{
    try {
        recrequest.listen();
    } catch (const std::exception& e) {
        TRACE(Trace::error, e.what());
        MSG(LTFSDMF0026E);
        THROW(Error::GENERAL_ERROR);
    }
}

void Connector::endTransRecalls()

{
    recrequest.closeRef();
}

Connector::rec_info_t Connector::getEvents()

{
    Connector::rec_info_t recinfo;
    long key;

    recrequest.accept();

    try {
        recrequest.recv();
    } catch (const std::exception& e) {
        MSG(LTFSDMF0019E, e.what(), errno);
        THROW(Error::GENERAL_ERROR);
    }

    const LTFSDmProtocol::LTFSDmTransRecRequest request =
            recrequest.transrecrequest();

    key = request.key();
    if (FuseConnector::ltfsdmKey != key) {
        TRACE(Trace::error, (long ) FuseConnector::ltfsdmKey, key);
        recinfo = (rec_info_t ) { NULL, false, (fuid_t ) { 0, 0, 0, 0 }, "" };
        return recinfo;
    }

    struct conn_info_t *conn_info = new struct conn_info_t;
    conn_info->reqrequest = new LTFSDmCommServer(recrequest);

    recinfo.conn_info = conn_info;
    recinfo.toresident = request.toresident();
    recinfo.fuid = (fuid_t ) { (unsigned long) request.fsidh(),
                    (unsigned long) request.fsidl(),
                    (unsigned int) request.igen(),
                    (unsigned long) request.inum() };
    recinfo.filename = request.filename();

    TRACE(Trace::always, recinfo.filename, recinfo.fuid.inum,
            recinfo.toresident);

    return recinfo;
}

void Connector::respondRecallEvent(rec_info_t recinfo, bool success)

{
    LTFSDmProtocol::LTFSDmTransRecResp *trecresp =
            recinfo.conn_info->reqrequest->mutable_transrecresp();

    trecresp->set_success(success);

    try {
        recinfo.conn_info->reqrequest->send();
    } catch (const std::exception& e) {
        TRACE(Trace::error, e.what());
        MSG(LTFSDMS0007E);
    }

    TRACE(Trace::always, recinfo.filename, success);

    recinfo.conn_info->reqrequest->closeAcc();

    delete (recinfo.conn_info->reqrequest);
    delete (recinfo.conn_info);
}

void Connector::terminate()

{
    Connector::connectorTerminate = true;

    LTFSDmCommClient commCommand(Const::RECALL_SOCKET_FILE);

    try {
        commCommand.connect();
    } catch (const std::exception& e) {
        MSG(LTFSDMF0020E, e.what(), errno);
        THROW(Error::GENERAL_ERROR);
    }

    LTFSDmProtocol::LTFSDmTransRecRequest *recrequest =
            commCommand.mutable_transrecrequest();

    recrequest->set_key(FuseConnector::ltfsdmKey);
    recrequest->set_toresident(false);
    recrequest->set_fsidh(0);
    recrequest->set_fsidl(0);
    recrequest->set_igen(0);
    recrequest->set_inum(0);
    recrequest->set_filename("");

    try {
        commCommand.send();
    } catch (const std::exception& e) {
        MSG(LTFSDMF0024E);
        THROW(Error::GENERAL_ERROR, errno);
    }
}

