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

namespace Const {
const int UNSET = -1;
const std::string SERVER_COMMAND = "ltfsdmd";
const std::string OVERLAY_FS_COMMAND = "ltfsdmd.ofs";
const int OUTPUT_LINE_SIZE = 1024;
const std::string LTFSDM_TMP_DIR = "/var/run/ltfsdm";
const std::string LTFSDM_DATA_DIR = ".ltfsdm";
const std::string DELIM = "/";
const std::string SERVER_LOCK_FILE = LTFSDM_TMP_DIR + DELIM + SERVER_COMMAND
        + ".lock";
const std::string TRACE_FILE = LTFSDM_TMP_DIR + DELIM + "LTFSDM.trc";
const std::string LOG_FILE = LTFSDM_TMP_DIR + DELIM + "LTFSDM.log";
const std::string CLIENT_SOCKET_FILE = LTFSDM_TMP_DIR + DELIM
        + "LTFSDM.client.soc";
const std::string RECALL_SOCKET_FILE = LTFSDM_TMP_DIR + DELIM
        + "LTFSDM.recall.soc";
const std::string KEY_FILE = LTFSDM_TMP_DIR + DELIM + "LTFSDM.key";
const std::string DB_FILE = LTFSDM_TMP_DIR + DELIM + "LTFSDM.db";
const std::string CONFIG_FILE = "/etc/ltfsdm.conf";
const std::string TMP_CONFIG_FILE = "/etc/ltfsdm.tmp.conf";
//const std::string DB_FILE = ":memory:";
const int MAX_RECEIVER_THREADS = 64;
const int MAX_STUBBING_THREADS = 64;
const int MAX_PREMIG_THREADS = 16;
const int MAX_TRANSPARENT_RECALL_THREADS = 8192;
const std::chrono::seconds IDLE_THREAD_LIVE_TIME(10);
const int MAX_OBJECTS_SEND = 100000;
const int MAX_FUSE_BACKGROUND = 256 * 1024;
const struct rlimit NOFILE_LIMIT = (struct rlimit ) { 1024 * 1024, 1024 * 1024 };
const struct rlimit NPROC_LIMIT = (struct rlimit ) { 16 * 1024 * 1024, 16 * 1024
                * 1024 };
const std::string DMAPI_SESSION_NAME = "ltfsdm";
const int LTFS_OPERATION_RETRY = 10;
const std::string LTFS_NAME = "ltfsdm";
const std::string LTFS_SYNC_VAL = "1";
const std::string DMAPI_ATTR_MIG = "LTFSDMMIG";
const std::string DMAPI_ATTR_FS = "LTFSDMFS";
const std::string LTFS_ATTR = "user.FILE_PATH";
const std::string LTFS_START_BLOCK = "user.ltfs.startblock";
const int READ_BUFFER_SIZE = 512 * 1024;
const long UPDATE_SIZE = 200 * 1024 * 1024;
const int maxReplica = 3;
const int tapeIdLength = 8;
const std::string DMAPI_TERMINATION_MESSAGE = "termination message";
const std::string FAILED_TAPE_ID = "FAILED";
const std::string LTFSDM_EA = ".ltfsdm.";
const std::string LTFSDM_EA_MIGSTATE = "trusted.ltfsdm.migstate";
const std::string LTFSDM_EA_MIGINFO = "trusted.ltfsdm.miginfo";
const std::string LTFSDM_EA_FSINFO = "trusted.ltfsdm.fsinfo";
const std::string LTFSDM_CACHE_DIR = "/.cache";
const std::string LTFSDM_CACHE_MP = LTFSDM_CACHE_DIR + "/...";
const std::string LTFSDM_IOCTL = LTFSDM_CACHE_DIR + "/ioctl";
const std::string LTFSDM_LOCK_DIR = LTFSDM_CACHE_DIR + "/locks";
const std::string TMP_DIR_TEMPLATE = "/tmp/ltfsdm.XXXXXX";
const std::string LTFSLE_HOST = "127.0.0.1";
const unsigned short int LTFSLE_PORT = 7600;
const int WAIT_TAPE_MOUNT = 60;
const int STARTUP_TIMEOUT = 720;
const int COMMAND_PARTIALLY_FAILED = 1;
const int COMMAND_FAILED = 2;
}
