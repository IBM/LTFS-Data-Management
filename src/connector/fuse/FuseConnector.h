#pragma once

/**
    @brief Global variables available within the Fuse Connector
 */
namespace FuseConnector {
extern std::mutex mtx;
extern std::map<std::string, std::unique_ptr<FuseFS>> managedFss;
extern long ltfsdmKey;
}
;
