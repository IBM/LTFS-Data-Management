#pragma once

namespace Const {
const int UNSET = -1;
const std::string SERVER_COMMAND = "ltfsdmd";
const std::string OVERLAY_FS_COMMAND = "ltfsdmd.ofs";
const int OUTPUT_LINE_SIZE = 1024;
const std::string LTFSDM_TMP_DIR = "/var/run/ltfsdm";
const std::string DELIM = "/";
const std::string SERVER_LOCK_FILE = LTFSDM_TMP_DIR + DELIM + SERVER_COMMAND + ".lock";
const std::string TRACE_FILE = LTFSDM_TMP_DIR + DELIM + "OpenLTFS.trc";
const std::string LOG_FILE = LTFSDM_TMP_DIR + DELIM + "OpenLTFS.log";
const std::string CLIENT_SOCKET_FILE = LTFSDM_TMP_DIR + DELIM + "OpenLTFS.client.soc";
const std::string RECALL_SOCKET_FILE = LTFSDM_TMP_DIR + DELIM + "OpenLTFS.recall.soc";
const std::string KEY_FILE = LTFSDM_TMP_DIR + DELIM + "OpenLTFS.key";
const std::string DB_FILE = LTFSDM_TMP_DIR + DELIM + "OpenLTFS.db";
const std::string CONFIG_FILE = "/etc/ltfsdm.conf";
const std::string TMP_CONFIG_FILE = "/etc/ltfsdm.tmp.conf";
//const std::string DB_FILE = ":memory:";
const int MAX_RECEIVER_THREADS = 64;
const int NUM_STUBBING_THREADS = 64;
const int NUM_PREMIG_THREADS = 16;
const int MAX_TRANSPARENT_RECALL_THREADS = 8192;
const int MAX_OBJECTS_SEND = 100000;
const int MAX_FUSE_BACKGROUND = 256 * 1024;
const struct rlimit NOFILE_LIMIT = (struct rlimit ) { 1024 * 1024, 1024 * 1024 };
const struct rlimit NPROC_LIMIT = (struct rlimit ) { 16 * 1024 * 1024, 16 * 1024
                * 1024 };
const std::string DMAPI_SESSION_NAME = "ltfsdm";
const std::string LTFS_NAME = "ltfsdm";
const std::string LTFS_SYNC_ATTR = "user.ltfs.sync";
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
const std::string OPEN_LTFS_EA = ".openltfs.";
const std::string OPEN_LTFS_EA_MANAGED = "trusted.openltfs.ismanaged";
const std::string OPEN_LTFS_EA_MOUNTPT = "trusted.openltfs.mountpt";
const std::string OPEN_LTFS_EA_FSNAME = "trusted.openltfs.fsname";
const std::string OPEN_LTFS_EA_MIGINFO_INT = "trusted.openltfs.miginfo.int";
const std::string OPEN_LTFS_EA_MIGINFO_EXT = "trusted.openltfs.miginfo.ext";
const std::string OPEN_LTFS_EA_FSINFO = "trusted.openltfs.fsinfo";
}
