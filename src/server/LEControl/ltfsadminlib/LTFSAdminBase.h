/**
 *  @file   LTFSAdminBase.h
 *  @brief  Base class definition for LTFS admin library
 *  @author Atsushi Abe (piste@jp.ibm.com), IBM Tokyo Lab., Japan
 */

#pragma once

#include <string>

#include <stdarg.h>

#include "LTFSAdminLog.h"

namespace ltfsadmin {

/** All base calss for ltfsadmin library
 *
 *  This base class provides the functionality of logging. All classes in ltfsadminlib shall
 *  inherit this calss. Defeult log level is set to INFO.
 */
class LTFSAdminBase {
public:
	/** Get current log level
	 *  @return current log level
	 */
	static loglevel_t GetLogLevel()
	{
		if (logger_)
			return logger_->GetLogLevel();
		else
			return log_level_;
	}

	/** Set (Change) current log level
	 *  @param log_level new log level
	 */
	static void SetLogLevel(loglevel_t log_level)
	{
		if (logger_)
			logger_->SetLogLevel(log_level);
		else
			log_level_ = log_level;
	}

	/** Set (Change) current log level
	 *  @param log_level new log level
	 */
	static void SetLogger(LTFSAdminLog *logger) { logger_ = logger;}

protected:
	/** Logging funtion
	 *
	 *  This function prints log message to standard error when specified log level is
	 *  equal or smaller than current log level.
	 *
	 *  @param level Log level
	 *  @param s string to print
	 */
	void Log(loglevel_t level, std::string s);

	void Msg(loglevel_t level, std::string id, ...);
	void Msg(loglevel_t level, std::string id, va_list ap);


private:
	static loglevel_t log_level_;
	static LTFSAdminLog *logger_;
};

}
