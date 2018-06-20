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

class MessageParser

{
private:
    static const std::string ALL_REQUESTS;
    static const std::string INFO_ALL_REQUESTS;
    static const std::string INFO_ONE_REQUEST;
    static const std::string INFO_ALL_JOBS;
    static const std::string INFO_SEL_JOBS;

    static void getObjects(LTFSDmCommServer *command, long localReqNumber,
            unsigned long pid, long requestNumber, FileOperation *fopt,
            std::set<std::string> pools = {});
    static void reqStatusMessage(long key, LTFSDmCommServer *command,
            FileOperation *fopt);
    static void migrationMessage(long key, LTFSDmCommServer *command,
            long localReqNumber);
    static void selRecallMessage(long key, LTFSDmCommServer *command,
            long localReqNumber);
    static void requestNumber(long key, LTFSDmCommServer *command,
            long *localReqNumber);
    static void stopMessage(long key, LTFSDmCommServer *command,
            std::unique_lock<std::mutex> *reclock, long localReqNumber);
    static void statusMessage(long key, LTFSDmCommServer *command,
            long localReqNumber);
    static void addMessage(long key, LTFSDmCommServer *command,
            long localReqNumber, std::shared_ptr<Connector> connector);
    static void infoRequestsMessage(long key, LTFSDmCommServer *command,
            long localReqNumber);
    static void infoJobsMessage(long key, LTFSDmCommServer *command,
            long localReqNumber);
    static void infoDrivesMessage(long key, LTFSDmCommServer *command);
    static void infoTapesMessage(long key, LTFSDmCommServer *command);
    static void poolCreateMessage(long key, LTFSDmCommServer *command);
    static void poolDeleteMessage(long key, LTFSDmCommServer *command);
    static void poolAddMessage(long key, LTFSDmCommServer *command);
    static void poolRemoveMessage(long key, LTFSDmCommServer *command);
    static void infoPoolsMessage(long key, LTFSDmCommServer *command);
    static void retrieveMessage(long key, LTFSDmCommServer *command);
public:
    MessageParser()
    {
    }
    ~MessageParser()
    {
    }
    static void run(long key, LTFSDmCommServer command,
            std::shared_ptr<Connector> connector);
};
