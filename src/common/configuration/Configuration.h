#pragma once

class Configuration {
private:
    struct fsinfo {
        std::string source;
        std::string fstype;
        std::string uuid;
        std::string options;
    };
    std::map<std::string, std::set<std::string>> stgplist;
    std::map<std::string, fsinfo> fslist;
    void write();
    std::recursive_mutex mtx;

    std::string encode(std::string s);
    std::string decode(std::string s);

public:
    void read();
    void poolCreate(std::string poolName);
    void poolDelete(std::string poolName);
    void poolAdd(std::string poolName, std::string tapeId);
    void poolRemove(std::string poolName, std::string tapeId);
    std::set<std::string> getPool(std::string poolName);
    std::set<std::string> getPools();

    void addFs(FileSystems::fsinfo newfs);
    FileSystems::fsinfo getFs(std::string target);
    std::set<std::string> getFss();
};
