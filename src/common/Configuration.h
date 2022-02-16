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
#pragma once

class Configuration
{
private:
    struct fsinfo
    {
        std::string source;
        std::string fstype;
        std::string uuid;
        std::string options;
               bool automig;
               std::string pool;
    };
    std::map<std::string, std::set<std::string>> stgplist;
    std::map<std::string, fsinfo> fslist;
    void write();
    std::recursive_mutex mtx;

    std::string encode(std::string s);
    std::string decode(std::string s);

public:
    void read();
    void poolCreate(std::string poolName);
    void poolDelete(std::string poolName);
    void poolAdd(std::string poolName, std::string tapeId);
    void poolRemove(std::string poolName, std::string tapeId);
    std::set<std::string> getPool(std::string poolName);
    std::set<std::string> getPools();

    void addFs(FileSystems::fsinfo newfs);
    FileSystems::fsinfo getFs(std::string target);
    std::set<std::string> getFss();
};
