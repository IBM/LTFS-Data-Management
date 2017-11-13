#include <unistd.h>
#include <limits.h>
#include <dirent.h>
#include <sys/resource.h>
#include <sys/ioctl.h>

#include <atomic>
#include <map>
#include <mutex>
#include <thread>

#include "src/common/const/Const.h"

#include "src/common/comm/ltfsdm.pb.h"
#include "src/common/comm/LTFSDmComm.h"

#include "src/connector/fuse/FuseLock.h"
#include "src/connector/fuse/FuseFS.h"
#include "src/connector/fuse/FuseConnector.h"

std::mutex FuseConnector::mtx;
std::map<std::string, std::unique_ptr<FuseFS>> FuseConnector::managedFss;
std::unique_lock<std::mutex> *FuseConnector::trecall_lock;
long FuseConnector::ltfsdmKey;
