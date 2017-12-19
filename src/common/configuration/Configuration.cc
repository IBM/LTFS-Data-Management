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

#include "src/common/errors/errors.h"
#include "src/common/exception/LTFSDMException.h"
#include "src/common/util/util.h"
#include "src/common/util/FileSystems.h"
#include "src/common/messages/Message.h"
#include "src/common/tracing/Trace.h"
#include "src/common/const/Const.h"

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

        conffiletmp << messages[LTFSDMX0032I] << std::endl;

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
                    << fs.second.options << " " << fs.second.uuid << std::endl;
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
    fs.options = it->second.options;
    fs.uuid = it->second.uuid;

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
