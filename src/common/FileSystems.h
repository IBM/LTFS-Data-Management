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

#ifndef TRUE
#define TRUE 1
#endif

#ifndef FALSE
#define FALSE 0
#endif

class FileSystems
{
public:
    struct fsinfo
    {
        std::string source;
        std::string target;
        std::string fstype;
        std::string uuid;
        std::string options;
    };
private:
    bool first;
    struct libmnt_context *cxt; // = mnt_new_context();
    struct libmnt_table *tb;
    blkid_cache cache;
    fsinfo getContext(struct libmnt_fs *mntfs);
    void getTable();
public:
    enum umountflag
    {
        UMNT_NORMAL, UMNT_DETACHED, UMNT_FORCED, UMNT_DETACHED_FORCED,
    };
    enum mountflag
    {
        MNT_NORMAL, MNT_FAKE,
    };
    FileSystems();
    ~FileSystems();
    FileSystems::fsinfo getByTarget(std::string target);
    void mount(std::string source, std::string target, std::string options,
            mountflag flag);
    void umount(std::string target, umountflag flag);
};
