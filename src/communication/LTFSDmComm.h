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

extern std::atomic<bool> exitClient;

class LTFSDmComm: public LTFSDmProtocol::Command
{
protected:
    std::string sockFile;
public:
    LTFSDmComm(const std::string _sockFile) :
            sockFile(_sockFile)
    {
    }
    ~LTFSDmComm()
    {
    }
    void send(int fd);
    void recv(int fd);
};

class LTFSDmCommClient: public LTFSDmComm
{
private:
    std::atomic<int> socRefFd;
public:
    LTFSDmCommClient(const std::string _sockFile) :
            LTFSDmComm(_sockFile), socRefFd(Const::UNSET)

    {
    }
    ~LTFSDmCommClient()
    {
        if (socRefFd != Const::UNSET)
            close(socRefFd);
    }
    void connect();
    void send()
    {
        return LTFSDmComm::send(socRefFd);
    }
    void recv()
    {
        return LTFSDmComm::recv(socRefFd);
    }
};

class LTFSDmCommServer: public LTFSDmComm
{
private:
    std::atomic<int> socRefFd;
    std::atomic<int> socAccFd;
public:
    LTFSDmCommServer(const std::string _sockFile) :
            LTFSDmComm(_sockFile), socRefFd(Const::UNSET), socAccFd(
                    Const::UNSET)
    {
    }
    LTFSDmCommServer(const LTFSDmCommServer& command) :
            LTFSDmComm(command.sockFile), socRefFd((int) command.socRefFd), socAccFd(
                    (int) command.socAccFd)

    {
    }
    ~LTFSDmCommServer()
    {
    }
    void listen();
    void accept();
    void closeAcc()
    {
        ::close(socAccFd);
        socAccFd = Const::UNSET;
    }
    void closeRef()
    {
        ::close(socRefFd);
        socRefFd = Const::UNSET;
    }
    void send()
    {
        return LTFSDmComm::send(socAccFd);
    }
    void recv()
    {
        return LTFSDmComm::recv(socAccFd);
    }
};
