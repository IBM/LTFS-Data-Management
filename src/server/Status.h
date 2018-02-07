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

class Status
{
private:
    struct singleState
    {
        long resident = 0;
        long premigrated = 0;
        long migrated = 0;
        long failed = 0;
    };
    std::map<int, singleState> allStates;
    std::mutex mtx;

    static const std::string STATUS;
public:
    Status()
    {
    }
    void add(int reqNumber);
    void remove(int reqNumber);
    void updateSuccess(int reqNumber, FsObj::file_state from,
            FsObj::file_state to);
    void updateFailed(int reqNumber, FsObj::file_state from);
    void get(int reqNumber, long *resident, long *premigrated, long *migrated,
            long *failed);
};

extern Status mrStatus;
