#ifndef _CONST_H
#define _CONST_H

namespace Const {
	const int UNSET = -1;
	const std::string SERVER_COMMAND = "ltfsdmd";
	const int OUTPUT_LINE_SIZE = 1024;
	const std::string LTFSDM_TMP_DIR =  std::string("/var/run/ltfsdm");
	const std::string DELIM = std::string("/");
	const std::string SERVER_LOCK_FILE = LTFSDM_TMP_DIR + DELIM + SERVER_COMMAND + std::string(".lock");
	const std::string TRACE_FILE = LTFSDM_TMP_DIR + DELIM + std::string("OpenLTFS.trc");
	const std::string LOG_FILE = LTFSDM_TMP_DIR + DELIM + std::string("OpenLTFS.log");
	const std::string SOCKET_FILE = LTFSDM_TMP_DIR + DELIM + std::string("OpenLTFS.soc");
	const std::string KEY_FILE = LTFSDM_TMP_DIR + DELIM + std::string("OpenLTFS.key");
	const std::string DB_FILE = LTFSDM_TMP_DIR + DELIM + std::string("OpenLTFS.db");
	const std::string CONFIG_FILE = std::string("/etc/ltfsdm.conf");
	const std::string TMP_CONFIG_FILE = std::string("/etc/ltfsdm.tmp.conf");
	//const std::string DB_FILE = std::string(":memory:");
	const int MAX_RECEIVER_THREADS = 2000;
	const int NUM_STUBBING_THREADS = 64;
	const int NUM_PREMIG_THREADS = 16;
	const int MAX_TRANSPARENT_RECALL_THREADS = 8192;
	const int MAX_OBJECTS_SEND = 100000;
	const int MAX_FUSE_BACKGROUND = 256*1024;
	const struct rlimit NOFILE_LIMIT = (struct rlimit) {1024*1024, 1024*1024};
	const struct rlimit NPROC_LIMIT = (struct rlimit) {16*1024*1024, 16*1024*1024};
	const std::string DMAPI_SESSION_NAME = std::string("ltfsdm");
	const std::string LTFS_PATH = std::string("/mnt/ltfs");
	const std::string LTFS_NAME = std::string("ltfsdm");
	const std::string LTFS_SYNC_ATTR = std::string("user.ltfs.sync");
	const std::string LTFS_SYNC_VAL = std::string("1");
	const std::string DMAPI_ATTR_MIG = std::string("LTFSDMMIG");
	const std::string DMAPI_ATTR_FS = std::string("LTFSDMFS");
	const std::string LTFS_ATTR = std::string("user.FILE_PATH");
	const std::string LTFS_START_BLOCK = std::string("user.ltfs.startblock");
	const int READ_BUFFER_SIZE = 512*1024;
	const long UPDATE_SIZE = 200*1024*1024;
	const int maxReplica = 3;
	const int tapeIdLength = 8;
	const std::string DMAPI_TERMINATION_MESSAGE = std::string("termination message");
	const std::string FAILED_TAPE_ID  = std::string("FAILED");
	const std::string OPEN_LTFS_EA = std::string(".openltfs.");
	const std::string OPEN_LTFS_EA_MANAGED = std::string("trusted.openltfs.ismanaged");
	const std::string OPEN_LTFS_EA_MOUNTPT = std::string("trusted.openltfs.mountpt");
	const std::string OPEN_LTFS_EA_FSNAME = std::string("trusted.openltfs.fsname");
	const std::string OPEN_LTFS_EA_MIGINFO_INT = std::string("trusted.openltfs.miginfo.int");
	const std::string OPEN_LTFS_EA_MIGINFO_EXT = std::string("trusted.openltfs.miginfo.ext");
	const std::string OPEN_LTFS_EA_FSINFO = std::string("trusted.openltfs.fsinfo");
}

#endif /* _CONST_H */
