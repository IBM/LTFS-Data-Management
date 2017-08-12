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
        LTFSDmComm(_sockFile),
        socRefFd(Const::UNSET)

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
        LTFSDmComm(_sockFile), socRefFd(Const::UNSET), socAccFd(Const::UNSET)
    {
    }
    LTFSDmCommServer(const LTFSDmCommServer& command) :
        LTFSDmComm(command.sockFile), socRefFd((int) command.socRefFd),
        socAccFd((int) command.socAccFd)

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
