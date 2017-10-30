#pragma once

class FileSystems {
public:
    struct fsinfo_t {
        std::string source;
        std::string target;
        std::string fstype;
        std::string uuid;
        std::string options;
    };
    static std::vector<FileSystems::fsinfo_t> getAll();
    static FileSystems::fsinfo_t getByTarget(std::string target);
};
