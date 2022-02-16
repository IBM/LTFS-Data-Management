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
#include <sys/resource.h>
#include <libmount/libmount.h>
#include <blkid/blkid.h>

#include <string>
#include <sstream>
#include <vector>
#include <set>
#include <map>
#include <mutex>

#include "src/common/errors.h"
#include "src/common/LTFSDMException.h"
#include "src/common/util.h"
#include "src/common/FileSystems.h"
#include "src/common/Message.h"
#include "src/common/Trace.h"
#include "src/common/Const.h"

#include "Configuration.h"

std::string Configuration::encode(std::string s)

{
    std::string enc;

    for (char c : s) {
        switch (c) {
            case 0040:
                enc += "\\0040";
                break;
            case 0134:
                enc += "\\0134";
                break;
            case 0012:
                enc += "\\0012";
                break;
            default:
                enc += c;
        }
    }

    return enc;
}

std::string Configuration::decode(std::string s)

{
    unsigned long pos = s.size();

    while ((pos = s.rfind("\\", pos)) != std::string::npos) {
        if (s.compare(pos, 5, "\\0040") == 0) {
            s.replace(pos, 5, std::string(1, 0040));
        } else if (s.compare(pos, 5, "\\0134") == 0) {
            s.replace(pos, 5, std::string(1, 0134));
        } else if (s.compare(pos, 5, "\\0012") == 0) {
            s.replace(pos, 5, std::string(1, 0012));
        } else {
            THROW(Error::CONFIG_FORMAT_ERROR);
        }
        pos--;
    }

    return s;
}

void Configuration::write()

{
    std::lock_guard<std::recursive_mutex> lock(mtx);

    {
        std::ofstream conffiletmp(Const::TMP_CONFIG_FILE, conffiletmp.trunc);

        conffiletmp << ltfsdm_messages[LTFSDMX0032I] << std::endl;

        for (std::pair<std::string, std::set<std::string>> pool : stgplist) {
            conffiletmp << "pool: " << encode(pool.first);
            for (std::string tapeId : pool.second) {
                conffiletmp << " " << tapeId;
            }
            conffiletmp << std::endl;
        }

        for (std::pair<std::string, fsinfo> fs : fslist) {
            conffiletmp << "fsys: " << encode(fs.first) << " "
                    << fs.second.source << " " << fs.second.fstype << " "
                    << fs.second.options << " " << fs.second.uuid << " "
                                       << (fs.second.automig == true ? "true" : "false") << " "
                               << (fs.second.pool == "" ? "-" : fs.second.pool ) << std::endl;
        }
    }

    if (rename((Const::TMP_CONFIG_FILE).c_str(), (Const::CONFIG_FILE).c_str())
            == -1)
        MSG(LTFSDMS0062E);
}

void Configuration::read()

{
    std::fstream conffile(Const::CONFIG_FILE);
    std::map<std::string, std::set<std::string>> stgplisttmp;
    std::map<std::string, fsinfo> fslisttmp;
    std::string line;
    std::string poolName;
    std::string fsName;
    fsinfo finfo;

    std::lock_guard<std::recursive_mutex> lock(mtx);

    while (std::getline(conffile, line)) {
        std::istringstream liness(line);
        std::string token;

        if (line[0] == '#')
            continue;

        if (!std::getline(liness, token, ' '))
            THROW(Error::CONFIG_FORMAT_ERROR);

        if (token.compare("pool:") == 0) {
            if (!std::getline(liness, token, ' '))
                THROW(Error::CONFIG_FORMAT_ERROR);
            poolName = decode(token);
            stgplisttmp[poolName] = {};
            while (std::getline(liness, token, ' '))
                stgplisttmp[poolName].insert(token);

        } else if (token.compare("fsys:") == 0) {
            if (!std::getline(liness, token, ' '))
                THROW(Error::CONFIG_FORMAT_ERROR);
            fsName = decode(token);
            if (!std::getline(liness, token, ' '))
                THROW(Error::CONFIG_FORMAT_ERROR);
            finfo.source = token;
            if (!std::getline(liness, token, ' '))
                THROW(Error::CONFIG_FORMAT_ERROR);
            finfo.fstype = token;
            if (!std::getline(liness, token, ' '))
                THROW(Error::CONFIG_FORMAT_ERROR);
            finfo.options = token;
            if (!std::getline(liness, token, ' '))
                THROW(Error::CONFIG_FORMAT_ERROR);
            finfo.uuid = token;
            if (!std::getline(liness, token, ' '))
                THROW(Error::CONFIG_FORMAT_ERROR);
                       finfo.automig = (token == "true"? true : false);
            if (!std::getline(liness, token, ' '))
                               THROW(Error::CONFIG_FORMAT_ERROR);
                       if (token == "-") {
                               finfo.pool = "";
                       } else {
                               finfo.pool = token;
                       }
                       if (std::getline(liness, token, ' '))
                               THROW(Error::CONFIG_FORMAT_ERROR);
            fslisttmp[fsName] = finfo;
        } else {
            THROW(Error::CONFIG_FORMAT_ERROR);
        }
    }

    stgplist = stgplisttmp;
    fslist = fslisttmp;
}

void Configuration::poolCreate(std::string poolName)

{
    std::lock_guard<std::recursive_mutex> lock(mtx);

    if (stgplist.find(poolName) != stgplist.end())
        THROW(Error::CONFIG_POOL_EXISTS);

    stgplist[poolName] = {};

    write();
}

void Configuration::poolDelete(std::string poolName)

{
    std::map<std::string, std::set<std::string>>::iterator it;

    std::lock_guard<std::recursive_mutex> lock(mtx);

    if ((it = stgplist.find(poolName)) == stgplist.end())
        THROW(Error::CONFIG_POOL_NOT_EXISTS);

    if (stgplist[poolName].size() != 0)
        THROW(Error::CONFIG_POOL_NOT_EMPTY);

    stgplist.erase(it);

    write();
}

void Configuration::poolAdd(std::string poolName, std::string tapeId)

{
    std::map<std::string, std::set<std::string>>::iterator it;

    std::lock_guard<std::recursive_mutex> lock(mtx);

    if ((it = stgplist.find(poolName)) == stgplist.end())
        THROW(Error::CONFIG_POOL_NOT_EXISTS);

    for (std::pair<std::string, std::set<std::string>> pool : stgplist)
        if (pool.second.find(tapeId) != pool.second.end())
            THROW(Error::CONFIG_TAPE_EXISTS);

    (*it).second.insert(tapeId);

    write();
}

void Configuration::poolRemove(std::string poolName, std::string tapeId)

{
    std::map<std::string, std::set<std::string>>::iterator itp;
    std::set<std::string>::iterator itt;

    std::lock_guard<std::recursive_mutex> lock(mtx);

    if ((itp = stgplist.find(poolName)) == stgplist.end())
        THROW(Error::CONFIG_POOL_NOT_EXISTS);

    if ((itt = (*itp).second.find(tapeId)) == (*itp).second.end())
        THROW(Error::CONFIG_TAPE_NOT_EXISTS);

    (*itp).second.erase(itt);

    write();
}

std::set<std::string> Configuration::getPool(std::string poolName)

{
    std::map<std::string, std::set<std::string>>::iterator it;

    std::lock_guard<std::recursive_mutex> lock(mtx);

    if ((it = stgplist.find(poolName)) == stgplist.end())
        THROW(Error::CONFIG_POOL_NOT_EXISTS);

    return it->second;
}

std::set<std::string> Configuration::getPools()

{
    std::set<std::string> poolnames;

    std::lock_guard<std::recursive_mutex> lock(mtx);

    for (std::pair<std::string, std::set<std::string>> pool : stgplist)
        poolnames.insert(pool.first);

    return poolnames;
}

void Configuration::addFs(FileSystems::fsinfo newfs)

{
    std::lock_guard<std::recursive_mutex> lock(mtx);

    if (fslist.find(newfs.target) != fslist.end())
        THROW(Error::CONFIG_TARGET_EXISTS);

    for (std::pair<std::string, fsinfo> fs : fslist) {
        if (fs.second.source.compare(newfs.source) == 0)
            THROW(Error::CONFIG_SOURCE_EXISTS);
        if (fs.second.uuid.compare(newfs.uuid) == 0)
            THROW(Error::CONFIG_UUID_EXISTS);
    }

    fslist[newfs.target].source = newfs.source;
    fslist[newfs.target].fstype = newfs.fstype;
    fslist[newfs.target].options = newfs.options;
    fslist[newfs.target].uuid = newfs.uuid;
       fslist[newfs.target].automig = newfs.automig;
       fslist[newfs.target].pool = newfs.pool;

    write();
}

FileSystems::fsinfo Configuration::getFs(std::string target)

{
    std::map<std::string, fsinfo>::iterator it;
    FileSystems::fsinfo fs;

    std::lock_guard<std::recursive_mutex> lock(mtx);

    if ((it = fslist.find(target)) == fslist.end())
        THROW(Error::CONFIG_TARGET_NOT_EXISTS);

    fs.source = it->second.source;
    fs.target = it->first;
    fs.fstype = it->second.fstype;
    fs.uuid = it->second.uuid;
    fs.options = it->second.options;
       fs.automig = it->second.automig;
       fs.pool = it->second.pool;

    return fs;
}

std::set<std::string> Configuration::getFss()

{
    std::set<std::string> fss;

    std::lock_guard<std::recursive_mutex> lock(mtx);

    for (std::pair<std::string, fsinfo> fs : fslist)
        fss.insert(fs.first);

    return fss;
}
