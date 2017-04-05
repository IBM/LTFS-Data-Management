/*
**  %Z% %I% %W% %G% %U%
**
**  ZZ_Copyright_BEGIN
**  ZZ_Copyright_END
**
*************************************************************************************
**
** COMPONENT NAME:  IBM Linear Tape File System
**
** FILE NAME:       LTFSAdminLog.h
**
** DESCRIPTION:     Base class for LTFS admin library logging facillity
**
** AUTHORS:         Atsushi Abe
**                  IBM Tokyo Lab., Japan
**                  piste@jp.ibm.com
**
*************************************************************************************
*/

/**
 *  @file   LTFSAdminLog.h
 *  @brief  Base class definition for LTFS admin library logging facility
 *  @author Atsushi Abe (piste@jp.ibm.com), IBM Tokyo Lab., Japan
 */

#pragma once

#include <string>

#include <stdarg.h>

#define MAX_LEN_MSG_BUF (29162 * 2 + 1024) /* hopefully enough size */

namespace ltfsadmin {

/** Emunumator of loglvel
 *
 *  Definition of log level
 */
enum loglevel_t {
	FATAL  = 0, /**< Fatal error: System cannot continue to run) */
	ERROR  = 1, /**< Normal error: System keep working after reporting error */
	WARN   = 2, /**< Warning: May be some action is needed but can be ignored  */
	INFO   = 3, /**< Information: Informative message */
	DEBUG0 = 4, /**< Debug: Debug Infomation for developer */
	DEBUG1 = 5, /**< Deteil Debug Information */
	DEBUG2 = 6, /**< More Detail Debug Information */
	DEBUG3 = 7  /**< Far Detail Debug Information */
};

class LTFSAdminLog {
public:
	/** Get current log level
	 *  @return current log level
	 */
	loglevel_t GetLogLevel() {return level_;}

	/** Set (Change) current log level
	 *  @param log_level new log level
	 */
	void SetLogLevel(loglevel_t log_level) {level_ = log_level;}

	/** Logging funtion
	 *
	 *  This function prints log message to standard error when specified log level is
	 *  equal or smaller than current log level.
	 *
	 *  @param level Log level
	 *  @param s string to print
	 */
	virtual void Log(loglevel_t level, std::string s) = 0;

	virtual void Msg(loglevel_t level, std::string id, ...) = 0;
	virtual void Msg(loglevel_t level, std::string id, va_list ap) = 0;

	static std::unordered_map<std::string, std::string> msg_table_;

private:
	loglevel_t level_;
};

}
