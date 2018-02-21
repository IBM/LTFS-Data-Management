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
class Server
{
private:
    SubServer subServer;
    long key;
    void lockServer();
    void writeKey();
    static void signalHandler(sigset_t set, long key);
public:
    static std::mutex termmtx;
    static std::condition_variable termcond;
    static std::atomic<bool> terminate;
    static std::atomic<bool> forcedTerminate;
    static std::atomic<bool> finishTerminate;

    static Configuration conf;

    static ThreadPool<Migration::mig_info_t,
            std::shared_ptr<std::list<unsigned long>>, FsObj::file_state> *wqs;

    static std::string getTapeName(FsObj *diskfile, std::string tapeId);
    static std::string getTapeName(unsigned long fsid_h, unsigned long fsid_l,
            unsigned int igen, unsigned long ino, std::string tapeId);
    static long getStartBlock(std::string tapeName);
    static void createDir(std::string path);
    static void createLink(std::string tapeId, std::string origPath,
            std::string dataPath);
    static void createDataDir(std::string tapeId);

    Server() :
            key(Const::UNSET)
    {
    }
    void initialize(bool dbUseMemory);
    void daemonize();
    void run(sigset_t set);
};
