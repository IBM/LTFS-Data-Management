#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/xattr.h>
#include <sys/stat.h>
#include <unistd.h>

#include <iostream>
#include <sstream>

#include <mutex>
#include <condition_variable>

#include <sqlite3.h>
#include "src/common/comm/ltfsdm.pb.h"

#include "src/common/util/util.h"
#include "src/common/messages/Message.h"
#include "src/common/tracing/Trace.h"
#include "src/common/errors/errors.h"
#include "src/common/const/Const.h"

#include "src/connector/Connector.h"

#include "DataBase.h"
#include "FileOperation.h"
#include "Scheduler.h"
#include "SelRecall.h"

long SelRecall::getStartBlock(std::string tapeName)

{
	long size;
	char startBlockStr[32];
	long startBlock;

	memset(startBlockStr, 0, sizeof(startBlockStr));

	size = getxattr(tapeName.c_str(), Const::LTFS_START_BLOCK.c_str(), startBlockStr, sizeof(startBlockStr));

	if ( size == -1 )
		return Const::UNSET;

	startBlock = strtol(startBlockStr, NULL, 0);

	if ( startBlock == LONG_MIN || startBlock == LONG_MAX )
		return Const::UNSET;
	else
		return startBlock;
}

void SelRecall::addFileName(std::string fileName)

{
	int rc;
	struct stat statbuf;
	std::stringstream ssql;
	sqlite3_stmt *stmt;
	std::string tapeName;
	std::string tapeId;
	int state;

	ssql << "INSERT INTO JOB_QUEUE (OPERATION, FILE_NAME, REQ_NUM, TARGET_STATE, FILE_SIZE, "
		 << "FS_ID, I_GEN, I_NUM, MTIME, LAST_UPD, FILE_STATE, TAPE_ID, START_BLOCK, FAILED) "
		 << "VALUES (" << DataBase::SELRECALL << ", "            // OPERATION
		 << "'" << fileName << "', "                             // FILE_NAME
		 << reqNumber << ", "                                    // REQ_NUM
		 << targetState << ", ";                                 // MIGRATION_STATE

	try {
		FsObj fso(fileName);
		statbuf = fso.stat();

		if (!S_ISREG(statbuf.st_mode)) {
			MSG(LTFSDMS0018E, fileName.c_str());
			return;
		}

		ssql << statbuf.st_size << ", "                          // FILE_SIZE
			 << fso.getFsId() << ", "                            // FS_ID
			 << fso.getIGen() << ", "                            // I_GEN
			 << fso.getINode() << ", "                           // I_NUM
			 << statbuf.st_mtime << ", "                         // MTIME
			 << time(NULL) << ", ";                              // LAST_UPD
		state = fso.getMigState();
		if ( fso.getMigState() == FsObj::RESIDENT ) {
			MSG(LTFSDMS0026I, fileName.c_str());
			return;
		}
		ssql << state << ", ";                                   // FILE_STATE
		tapeId = fso.getAttribute(Const::DMAPI_ATTR);
		ssql << "'" << tapeId << "', ";                          // TAPE_ID
		tapeName = Scheduler::getTapeName(fileName, tapeId);
		ssql << getStartBlock(tapeName) << ", "                  // START_BLOCK
			 << 0 << ");";                                       // FAILED
	}
	catch ( int error ) {
		MSG(LTFSDMS0017E, fileName.c_str());
	}

	sqlite3_statement::prepare(ssql.str(), &stmt);

	rc = sqlite3_statement::step(stmt);

	sqlite3_statement::checkRcAndFinalize(stmt, rc, SQLITE_DONE);

	return;
}

void SelRecall::addRequest()

{
	int rc;
	std::stringstream ssql;
	sqlite3_stmt *stmt;

	ssql.str("");
	ssql.clear();

	ssql << "SELECT TAPE_ID FROM JOB_QUEUE WHERE REQ_NUM="
		 << reqNumber << " GROUP BY TAPE_ID";

	sqlite3_statement::prepare(ssql.str(), &stmt);

	while ( (rc = sqlite3_statement::step(stmt)) == SQLITE_ROW ) {
		std::unique_lock<std::mutex> lock(Scheduler::mtx);

		std::stringstream ssql2;
		sqlite3_stmt *stmt2;

		const char *cstr = reinterpret_cast<const char*>(sqlite3_column_text (stmt, 0));

		ssql2 << "INSERT INTO REQUEST_QUEUE (OPERATION, REQ_NUM, TARGET_STATE, "
			  << "COLOC_GRP, TAPE_ID, TIME_ADDED, STATE) "
			  << "VALUES (" << DataBase::SELRECALL << ", "                          // OPERATION
			  << reqNumber << ", "                                                  // FILE_NAME
			  << targetState << ", "                                                // TARGET_STATE
			  << "NULL" << ", "                                                     // COLOC_GRP
			  << "'" << (cstr ? std::string(cstr) : std::string("")) << "', "       // TAPE_ID
			  << time(NULL) << ", "                                                 // TIME_ADDED
			  << DataBase::REQ_NEW << ");";                                         // STATE

		sqlite3_statement::prepare(ssql2.str(), &stmt2);

		rc = sqlite3_statement::step(stmt2);

		sqlite3_statement::checkRcAndFinalize(stmt2, rc, SQLITE_DONE);

		Scheduler::cond.notify_one();
	}

	sqlite3_statement::checkRcAndFinalize(stmt, rc, SQLITE_DONE);
}


unsigned long recall(std::string fileName, std::string tapeId,
					 FsObj::file_state state, FsObj::file_state toState)

{
	struct stat statbuf;
	std::string tapeName;
	char buffer[Const::READ_BUFFER_SIZE];
	long rsize;
	long wsize;
	int fd = -1;
	long offset = 0;

	try {
		FsObj target(fileName);

		target.lock();

		if ( state == FsObj::MIGRATED ) {
			tapeName = Scheduler::getTapeName(fileName, tapeId);
			fd = open(tapeName.c_str(), O_RDWR);

			if ( fd == -1 ) {
				TRACE(Trace::error, errno);
				MSG(LTFSDMS0021E, tapeName.c_str());
				throw(Error::LTFSDM_GENERAL_ERROR);
			}

			statbuf = target.stat();

			while ( offset < statbuf.st_size ) {
				rsize = read(fd, buffer, sizeof(buffer));
				if ( rsize == -1 ) {
					TRACE(Trace::error, errno);
					MSG(LTFSDMS0023E, fileName);
					throw(errno);
				}
				wsize = target.write(offset, (unsigned long) rsize, buffer);
				if ( wsize != rsize ) {
					TRACE(Trace::error, errno);
					TRACE(Trace::error, wsize);
					TRACE(Trace::error, rsize);
					MSG(LTFSDMS0027E, fileName.c_str());
					close(fd);
					throw(Error::LTFSDM_GENERAL_ERROR);
				}
				offset += rsize;
			}

			close(fd);
		}

		target.finishRecall(toState);
		if ( toState == FsObj::RESIDENT )
			target.remAttribute(Const::DMAPI_ATTR);
		target.unlock();
	}
	catch ( int error ) {
		if ( fd != -1 )
			close(fd);
	}

	return statbuf.st_size;
}

void recallStep(int reqNum, std::string tapeId, FsObj::file_state toState)

{
	sqlite3_stmt *stmt;
	std::stringstream ssql;
	int rc, rc2;
    int group_end = 0;
    int group_begin = -1;
	int accumSize = 0;
	std::unique_lock<std::mutex> lock(Scheduler::updmtx);

	Scheduler::updReq = reqNum;
	Scheduler::updcond.notify_all();
	lock.unlock();

	ssql << "SELECT ROWID, FILE_NAME, FILE_STATE FROM JOB_QUEUE WHERE REQ_NUM="
		 << reqNum << " AND TAPE_ID='" << tapeId << "' ORDER BY START_BLOCK";

	sqlite3_statement::prepare(ssql.str(), &stmt);

	while ( (rc = sqlite3_statement::step(stmt) ) ) {
		if ( rc != SQLITE_ROW && rc != SQLITE_DONE )
			break;

		if ( rc == SQLITE_ROW ) {
			const char *cstr = reinterpret_cast<const char*>(sqlite3_column_text (stmt, 1));

			if (!cstr)
				continue;

			FsObj::file_state state = (FsObj::file_state) sqlite3_column_int(stmt, 2);

			if ( state == FsObj::RESIDENT )
				continue;

			if ( state == toState )
				continue;

			accumSize += recall(std::string(cstr), tapeId, state, toState);

			group_end = sqlite3_column_int(stmt, 0);
			if ( group_begin == -1 )
				group_begin = group_end;
			if ( accumSize < Const::UPDATE_SIZE )
				continue;
			accumSize = 0;
		}

		ssql.str("");
		ssql.clear();

		ssql << "UPDATE JOB_QUEUE SET FILE_STATE = " <<  toState
			 << " WHERE REQ_NUM=" << reqNum << " AND TAPE_ID ='" << tapeId
			 << "' AND (ROWID BETWEEN " << group_begin << " AND " << group_end << ")";

		sqlite3_stmt *stmt2;

		lock.lock();

		sqlite3_statement::prepare(ssql.str(), &stmt2);
		rc2 = sqlite3_statement::step(stmt2);
		sqlite3_statement::checkRcAndFinalize(stmt2, rc2, SQLITE_DONE);

		Scheduler::updReq = reqNum;
		Scheduler::updcond.notify_all();
		lock.unlock();

		group_begin = -1;

		if ( rc == SQLITE_DONE )
			break;
	}

	sqlite3_statement::checkRcAndFinalize(stmt, rc, SQLITE_DONE);
}

void SelRecall::execRequest(int reqNum, int tgtState, std::string tapeId)

{
	std::stringstream ssql;
	sqlite3_stmt *stmt;
	int rc;

	if ( tgtState == LTFSDmProtocol::LTFSDmMigRequest::PREMIGRATED )
		recallStep(reqNum, tapeId, FsObj::PREMIGRATED);
	else
		recallStep(reqNum, tapeId, FsObj::RESIDENT);

	std::unique_lock<std::mutex> lock(Scheduler::mtx);
	ssql << "UPDATE TAPE_LIST SET STATE=" << DataBase::TAPE_FREE
		 << " WHERE TAPE_ID='" << tapeId << "';";
	sqlite3_statement::prepare(ssql.str(), &stmt);
	rc = sqlite3_statement::step(stmt);
	sqlite3_statement::checkRcAndFinalize(stmt, rc, SQLITE_DONE);
	Scheduler::cond.notify_one();

	std::unique_lock<std::mutex> updlock(Scheduler::updmtx);

	ssql.str("");
	ssql.clear();
	ssql << "UPDATE REQUEST_QUEUE SET STATE=" << DataBase::REQ_COMPLETED
		 << " WHERE REQ_NUM=" << reqNum << " AND TAPE_ID='" << tapeId << "';";
	sqlite3_statement::prepare(ssql.str(), &stmt);
	rc = sqlite3_statement::step(stmt);
	sqlite3_statement::checkRcAndFinalize(stmt, rc, SQLITE_DONE);

	Scheduler::updcond.notify_one();
}
