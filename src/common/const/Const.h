#ifndef _CONST_H
#define _CONST_H

namespace Const {
	const std::string SERVER_COMMAND = "ltfsdmd";
	const int OUTPUT_LINE_SIZE = 1024;
	const std::string SERVER_LOCK_FILE = std::string("/tmp/") + SERVER_COMMAND + std::string(".lock");
	const std::string TRACE_FILE = std::string("/tmp/OpenLTFS.trc");
	const std::string LOG_FILE = std::string("/tmp/OpenLTFS.log");
	const std::string SOCKET_FILE = std::string("/tmp/OpenLTFS.soc");
}

#endif /* _CONST_H */
