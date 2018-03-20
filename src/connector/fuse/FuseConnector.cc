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
#include <unistd.h>
#include <limits.h>
#include <dirent.h>
#include <sys/resource.h>
#include <sys/ioctl.h>

#include <atomic>
#include <map>
#include <mutex>
#include <thread>

#include "src/common/Const.h"

#include "src/communication/ltfsdm.pb.h"
#include "src/communication/LTFSDmComm.h"

#include "src/connector/fuse/FuseLock.h"
#include "src/connector/fuse/FuseFS.h"
#include "src/connector/fuse/FuseConnector.h"

std::mutex FuseConnector::mtx;
std::map<std::string, std::unique_ptr<FuseFS>> FuseConnector::managedFss;
long FuseConnector::ltfsdmKey;
