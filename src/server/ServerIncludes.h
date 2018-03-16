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

#include <unistd.h>
#include <unistd.h>
#include <signal.h>
#include <fcntl.h>
#include <limits.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/xattr.h>
#include <sys/resource.h>
#include <sys/file.h>
#include <libmount/libmount.h>
#include <blkid/blkid.h>
#include <errno.h>

#include <string>
#include <sstream>
#include <memory>
#include <list>
#include <condition_variable>
#include <unordered_map>
#include <thread>
#include <mutex>
#include <map>
#include <set>
#include <vector>
#include <future>

#include <sqlite3.h>

#include "src/common/util/util.h"
#include "src/common/util/FileSystems.h"
#include "src/common/messages/Message.h"
#include "src/common/tracing/Trace.h"
#include "src/common/errors/errors.h"
#include "src/common/const/Const.h"
#include "src/common/comm/ltfsdm.pb.h"
#include "src/common/comm/LTFSDmComm.h"
#include "src/common/exception/LTFSDMException.h"
#include "src/common/configuration/Configuration.h"

#include "src/connector/Connector.h"

#include "SubServer.h"
#include "ThreadPool.h"
#include "Status.h"
#include "DataBase.h"
#include "FileOperation.h"
#include "MessageParser.h"
#include "Receiver.h"
#include "Migration.h"
#include "SelRecall.h"
#include "TransRecall.h"
#include "Server.h"
#include "LTFSDMInventory.h"
#include "TapeMover.h"
#include "Scheduler.h"
