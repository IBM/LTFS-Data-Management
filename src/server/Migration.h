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

class Migration: public FileOperation
{
private:
    unsigned long pid;
    int reqNumber;
    std::set<std::string> pools;
    int numReplica;
    int targetState;
    int jobnum;
    bool needsTape = false;

    struct req_return_t
    {
        bool remaining;
        bool suspended;
    };

    FsObj::file_state checkState(std::string fileName, FsObj *fso);

    static const std::string ADD_JOB;
    static const std::string ADD_REQUEST;
    static const std::string FAIL_PREMIGRATION;
    static const std::string FAIL_STUBBING;
    static const std::string SET_TRANSFERRING;
    static const std::string SET_CHANGE_STATE;
    static const std::string SELECT_JOBS;
    static const std::string SET_JOB_SUCCESS;
    static const std::string RESET_JOB_STATE;
    static const std::string FAIL_PREMIGRATED;
    static const std::string UPDATE_REQUEST;
    static const std::string UPDATE_REQUEST_RESET_TAPE;

    static ThreadPool<Migration, int, std::string, std::string, std::string, bool> swq;

    req_return_t processFiles(int replNum, std::string tapeId,
            FsObj::file_state fromState, FsObj::file_state toState);
public:
    struct mig_info_t
    {
        std::string fileName;
        int reqNumber;
        int numRepl;
        int replNum;
        unsigned long inum;
        std::string poolName;
        FsObj::file_state fromState;
        FsObj::file_state toState;
    };
    static std::mutex pmigmtx;

    static unsigned long transferData(std::string tapeId, std::string driveId,
            long secs, long nsecs, mig_info_t miginfo,
            std::shared_ptr<std::list<unsigned long>> inumList,
            std::shared_ptr<bool>);
    static void changeFileState(mig_info_t mig_info,
            std::shared_ptr<std::list<unsigned long>> inumList, FsObj::file_state toState);

    Migration(unsigned long _pid, long _reqNumber, std::set<std::string> _pools,
            int _numReplica, int _targetState) :
            pid(_pid), reqNumber(_reqNumber), pools(_pools), numReplica(
                    _numReplica), targetState(_targetState), jobnum(0)
    {
    }
    void addJob(std::string fileName);
    void addRequest();
    void execRequest(int replNum, std::string driveId, std::string pool, std::string tapeId,
            bool needsTape);
};
