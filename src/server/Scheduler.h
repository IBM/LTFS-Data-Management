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

class Scheduler

{
private:
    DataBase::operation op;
    int reqNum;
    int numRepl;
    int replNum;
    int tgtState;
    std::string tapeId;
    std::string driveId;
    std::string pool;
    SubServer subs;

    void makeUse(std::string driveId, std::string tapeId);
    void moveTape(std::string driveId, std::string tapeId, TapeMover::operation op);
    bool poolResAvail(unsigned long minFileSize);
    bool tapeResAvail();
    bool resAvail(unsigned long minFileSize);
    unsigned long smallestMigJob(int reqNum, int replNum);

    static const std::string SELECT_REQUEST;
    static const std::string UPDATE_MNT_REQUEST;
    static const std::string UPDATE_MIG_REQUEST;
    static const std::string UPDATE_REC_REQUEST;
    static const std::string SMALLEST_MIG_JOB;
public:
    static std::mutex mtx;
    static std::condition_variable cond;
    static std::mutex updmtx;
    static std::condition_variable updcond;
    static std::map<int, std::atomic<bool>> updReq;
    static std::map<std::string, std::atomic<bool>> suspend_map;

    static void mount(std::string driveid, std::string cartridgeid)
    {
        inventory->mount(driveid, cartridgeid);
    }
    static void unmount(std::string driveid, std::string cartridgeid)
    {
        inventory->unmount(driveid, cartridgeid);
    }
    Scheduler() :
            op(DataBase::NOOP), reqNum(Const::UNSET), numRepl(Const::UNSET), replNum(
                    Const::UNSET), tgtState(Const::UNSET)
    {
    }
    ~Scheduler()
    {
    }
    void run(long key);
};
