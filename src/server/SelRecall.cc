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

void SelRecall::addJob(std::string fileName)

{
	int rc;
	struct stat statbuf;
	std::stringstream ssql;
	sqlite3_stmt *stmt;
	std::string tapeName;
	int state;
	FsObj::mig_attr_t attr;

	ssql << "INSERT INTO JOB_QUEUE (OPERATION, FILE_NAME, REQ_NUM, TARGET_STATE, FILE_SIZE, FS_ID, I_GEN, "
		 << "I_NUM, MTIME_SEC, MTIME_NSEC, LAST_UPD, FILE_STATE, TAPE_ID, START_BLOCK, FAILED) "
		 << "VALUES (" << DataBase::SELRECALL << ", "            // OPERATION
		 << "'" << fileName << "', "                             // FILE_NAME
		 << reqNumber << ", "                                    // REQ_NUM
		 << targetState << ", ";                                 // MIGRATION_STATE

	try {
		FsObj fso(fileName);
		stat(fileName.c_str(), &statbuf);

		if (!S_ISREG(statbuf.st_mode)) {
			MSG(LTFSDMS0018E, fileName.c_str());
			return;
		}

		ssql << statbuf.st_size << ", "                          // FILE_SIZE
			 << (long long) fso.getFsId() << ", "                // FS_ID
			 << fso.getIGen() << ", "                            // I_GEN
			 << (long long) fso.getINode() << ", "               // I_NUM
			 << statbuf.st_mtim.tv_sec << ", "                   // MTIME_SEC
			 << statbuf.st_mtim.tv_nsec << ", "                  // MTIME_NSEC
			 << time(NULL) << ", ";                              // LAST_UPD
		state = fso.getMigState();
		if ( fso.getMigState() == FsObj::RESIDENT ) {
			MSG(LTFSDMS0026I, fileName.c_str());
			return;
		}
		ssql << state << ", ";                                   // FILE_STATE
		attr = fso.getAttribute();
		ssql << "'" << attr.tapeId[0] << "', ";                  // TAPE_ID
		tapeName = Scheduler::getTapeName(fileName, attr.tapeId[0]);
		ssql << Scheduler::getStartBlock(tapeName) << ", "       // START_BLOCK
			 << 0 << ");";                                       // FAILED
	}
	catch ( int error ) {
		ssql.str("");
		ssql.clear();
		ssql << "INSERT INTO JOB_QUEUE (OPERATION, FILE_NAME, REQ_NUM, TARGET_STATE, FILE_SIZE, FS_ID, I_GEN, "
			 << "I_NUM, MTIME_SEC, MTIME_NSEC, LAST_UPD, FILE_STATE, TAPE_ID, FAILED) "
			 << "VALUES (" << DataBase::SELRECALL << ", "        // OPERATION
			 << "'" << fileName << "', "                         // FILE_NAME
			 << reqNumber << ", "                                // REQ_NUM
			 << targetState << ", "                              // MIGRATION_STATE
			 << Const::UNSET  << ", "                            // FILE_SIZE
			 << Const::UNSET  << ", "                            // FS_ID
			 << Const::UNSET  << ", "                            // I_GEN
			 << Const::UNSET  << ", "                            // I_NUM
			 << Const::UNSET  << ", "                            // MTIME_SEC
			 << Const::UNSET  << ", "                            // MTIME_NSEC
			 << time(NULL) << ", "                               // LAST_UPD
			 << FsObj::FAILED  << ", "                           // FILE_STATE
			 << "'" << Const::FAILED_TAPE_ID << "'" << ", "      // TAPE_ID
			 << 0 << ");";
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

	Scheduler::updReq[reqNumber] = false;

	while ( (rc = sqlite3_statement::step(stmt)) == SQLITE_ROW ) {
		std::unique_lock<std::mutex> lock(Scheduler::mtx);

		std::stringstream ssql2;
		sqlite3_stmt *stmt2;

		const char *cstr = reinterpret_cast<const char*>(sqlite3_column_text (stmt, 0));
		std::string tapeId = std::string(cstr ? cstr : "");

		ssql2 << "INSERT INTO REQUEST_QUEUE (OPERATION, REQ_NUM, TARGET_STATE, "
			  << "COLOC_GRP, TAPE_ID, TIME_ADDED, STATE) "
			  << "VALUES (" << DataBase::SELRECALL << ", "                          // OPERATION
			  << reqNumber << ", "                                                  // REQ_NUM
			  << targetState << ", "                                                // TARGET_STATE
			  << "NULL" << ", "                                                     // COLOC_GRP
			  << "'" << tapeId << "', "       // TAPE_ID
			  << time(NULL) << ", ";                                                // TIME_ADDED
		if ( tapeId.compare(Const::FAILED_TAPE_ID) != 0 )
			ssql2 << DataBase::REQ_NEW << ");";                                     // STATE
		else
			ssql2 << DataBase::REQ_COMPLETED << ");";                               // STATE

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
	FsObj::file_state curstate;

	try {
		FsObj target(fileName);

		target.lock();

		curstate = target.getMigState();

		if ( curstate != state ) {
			MSG(LTFSDMS0035I, fileName);
			state = curstate;
		}
		if ( state == FsObj::RESIDENT ) {
			return 0;
		}
		else if ( state == FsObj::MIGRATED ) {
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
					MSG(LTFSDMS0023E,  tapeName.c_str());
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
			target.remAttribute();
		target.unlock();
	}
	catch ( int error ) {
		if ( fd != -1 )
			close(fd);
		throw(error);
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
	time_t start;

	std::unique_lock<std::mutex> lock(Scheduler::updmtx);
	TRACE(Trace::much, reqNum);
	Scheduler::updReq[reqNum] = true;
	Scheduler::updcond.notify_all();
	lock.unlock();

	ssql << "SELECT ROWID, FILE_NAME, FILE_STATE FROM JOB_QUEUE WHERE REQ_NUM="
		 << reqNum << " AND TAPE_ID='" << tapeId << "' ORDER BY START_BLOCK";

	sqlite3_statement::prepare(ssql.str(), &stmt);

	start = time(NULL);

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

			try {
				recall(std::string(cstr), tapeId, state, toState);
				group_end = sqlite3_column_int(stmt, 0);
			}
			catch (int error) {
				ssql.str("");
				ssql.clear();
				ssql << "UPDATE JOB_QUEUE SET FILE_STATE = " <<  FsObj::FAILED
					 << " WHERE REQ_NUM=" << reqNum
					 << " AND TAPE_ID ='" << tapeId << "'"
					 << " AND ROWID=" << sqlite3_column_int(stmt, 0);
				TRACE(Trace::error, ssql.str());
				sqlite3_stmt *stmt2;
				sqlite3_statement::prepare(ssql.str(), &stmt2);
				rc2 = sqlite3_statement::step(stmt2);
				sqlite3_statement::checkRcAndFinalize(stmt2, rc2, SQLITE_DONE);

				if ( group_begin == -1 ) {
					start = time(NULL);
					continue;
				}
				start = 0;
			}
			if ( group_begin == -1 )
				group_begin = group_end;
			if ( time(NULL) - start < 10 )
				continue;
			start = time(NULL);
		}

		ssql.str("");
		ssql.clear();

		ssql << "UPDATE JOB_QUEUE SET FILE_STATE = " <<  toState
			 << " WHERE REQ_NUM=" << reqNum
			 << " AND TAPE_ID ='" << tapeId << "'"
			 << " AND FILE_STATE!=" << FsObj::FAILED
			 << " AND (ROWID BETWEEN " << group_begin << " AND " << group_end << ")";

		sqlite3_stmt *stmt2;

		lock.lock();

		sqlite3_statement::prepare(ssql.str(), &stmt2);
		rc2 = sqlite3_statement::step(stmt2);
		sqlite3_statement::checkRcAndFinalize(stmt2, rc2, SQLITE_DONE);

		Scheduler::updReq[reqNum] = true;
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

	Scheduler::updReq[reqNum] = true;
	Scheduler::updcond.notify_all();
}
