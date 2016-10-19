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
	const int MAX_RECEIVER_THREADS = 40;
	const int MAX_OBJECTS_SEND = 11;
}

#endif /* _CONST_H */
