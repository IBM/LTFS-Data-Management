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

class Mount {
private:
    std::string driveId;
    std::string tapeId;
    int reqNum;

    static const std::string ADD_REQUEST;
    static const std::string DELETE_REQUEST;
public:
    enum operation {
        MOUNT,
        UNMOUNT
    };
private:
    operation op;
public:
    Mount(std::string _driveId, std::string _tapeId, int _reqNum, operation _op) : driveId(_driveId), tapeId(_tapeId), reqNum(_reqNum), op(_op) {}
    Mount(std::string _driveId, std::string _tapeId, operation _op) : driveId(_driveId), tapeId(_tapeId), reqNum(Const::UNSET), op(_op) {}
    long addRequest();
    void execRequest();
};
