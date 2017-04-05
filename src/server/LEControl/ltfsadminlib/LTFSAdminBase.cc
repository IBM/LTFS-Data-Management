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
** FILE NAME:       LTFSAdminBase.cc
**
** DESCRIPTION:     Base class for LTFS admin library
**
** AUTHORS:         Atsushi Abe
**                  IBM Tokyo Lab., Japan
**                  piste@jp.ibm.com
**
*************************************************************************************
*/

/**
 *  @file   LTFSAdminBase.cc
 *  @brief  Base class implementation for LTFS admin library
 *  @author Atsushi Abe (piste@jp.ibm.com), IBM Tokyo Lab., Japan
 */

#include <iostream>

#include "stdio.h"

#include "LTFSAdminBase.h"

#include <string.h>

using namespace ltfsadmin;

loglevel_t LTFSAdminBase::log_level_ = INFO;

LTFSAdminLog *LTFSAdminBase::logger_ = NULL;

void LTFSAdminBase::Log(loglevel_t level, string s)
{
	if (logger_)
		logger_->Log(level, s);
	else {
		if (level <= GetLogLevel()) {
			cerr << s;
			cerr << endl;
		}
	}
}

void LTFSAdminBase::Msg(loglevel_t level, string id, ...)
{
	if (logger_) {
		va_list ap;
		va_start(ap, id);
		logger_->Msg(level, id, ap);
		va_end(ap);
	} else {
		va_list ap;
		va_start(ap, id);
		Msg(level, id, ap);
		va_end(ap);
	}
}

void LTFSAdminBase::Msg(loglevel_t level, string id, va_list ap)
{
	if (LTFSAdminLog::msg_table_.count(id)) {
		if (level <= GetLogLevel()) {
			char arg[MAX_LEN_MSG_BUF + 1];
			string fmt = LTFSAdminLog::msg_table_[id];
			strncpy(arg, fmt.c_str(), MAX_LEN_MSG_BUF);
			vfprintf(stderr, arg, ap);
			fprintf(stderr, "\n");
		}
	}
}
