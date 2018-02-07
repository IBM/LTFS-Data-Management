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

class SelRecall: public FileOperation
{
private:
    unsigned long pid;
    long reqNumber;
    std::set<std::string> needsTape;
    int targetState;
    static unsigned long recall(std::string fileName, std::string tapeId,
            FsObj::file_state state, FsObj::file_state toState);
    bool processFiles(std::string tapeId, FsObj::file_state toState,
            bool needsTape);

    static const std::string ADD_JOB;
    static const std::string GET_TAPES;
    static const std::string ADD_REQUEST;
    static const std::string SET_RECALLING;
    static const std::string SELECT_JOBS;
    static const std::string FAIL_JOB;
    static const std::string SET_JOB_SUCCESS;
    static const std::string RESET_JOB_STATE;
    static const std::string UPDATE_REQUEST;
public:
    SelRecall(unsigned long _pid, long _reqNumber, int _targetState) :
            pid(_pid), reqNumber(_reqNumber), targetState(_targetState)
    {
    }
    void addJob(std::string fileName);
    void addRequest();
    void execRequest(std::string tapeId, bool needsTape);
};
