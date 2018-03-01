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

class TransRecall

{
private:
    static const std::string ADD_JOB;
    static const std::string CHECK_REQUEST_EXISTS;
    static const std::string CHANGE_REQUEST_TO_NEW;
    static const std::string ADD_REQUEST;
    static const std::string REMAINING_JOBS;
    static const std::string SET_RECALLING;
    static const std::string SELECT_JOBS;
    static const std::string DELETE_JOBS;
    static const std::string COUNT_REMAINING_JOBS;
    static const std::string DELETE_REQUEST;

    void processFiles(int reqNum, std::string tapeId);
public:
    TransRecall()
    {
    }
    ~TransRecall()
    {
    }
    void addJob(Connector::rec_info_t recinfo, std::string tapeId,
            long reqNum);
    void cleanupEvents();
    void run(std::shared_ptr<Connector> connector);
    static unsigned long recall(Connector::rec_info_t recinfo,
            std::string tapeId, FsObj::file_state state,
            FsObj::file_state toState);

    void execRequest(int reqNum, std::string driveId, std::string tapeId);
};
