#pragma once

namespace FuseConnector {
extern std::mutex mtx;
extern std::map<std::string, std::unique_ptr<FuseFS>> managedFss;
extern long ltfsdmKey;
}
;
