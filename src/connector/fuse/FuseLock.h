#pragma once

class FuseLock
{
public:
    enum lockOperation
    {
        lockshared, lockexclusive,
    };
    enum lockType
    {
        main = 'm', fuse = 'f',
    };
private:
    std::string id;
    int fd;
    FuseLock::lockType type;
    FuseLock::lockOperation operation;
    static std::mutex mtx;
public:
    FuseLock(std::string identifier, FuseLock::lockType _type,
            FuseLock::lockOperation _operation);
    ~FuseLock();
    void lock();
    bool try_lock();
    void unlock();
};
