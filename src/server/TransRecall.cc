#include <limits.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <map>
#include <atomic>
#include <condition_variable>
#include <mutex>
#include <thread>

#include <sqlite3.h>

#include "src/common/util/util.h"
#include "src/common/messages/Message.h"
#include "src/common/tracing/Trace.h"
#include "src/common/errors/errors.h"
#include "src/common/const/Const.h"

#include "src/connector/Connector.h"

#include "DataBase.h"
#include "Receiver.h"
#include "SubServer.h"
#include "Server.h"
#include "Scheduler.h"
#include "TransRecall.h"


void TransRecall::recall(Connector::rec_info_t recinfo, std::string tapeId, long reqNum, bool newReq)

{
	int rc;
	struct stat statbuf;
	std::stringstream ssql;
	sqlite3_stmt *stmt;
	std::string tapeName;
	int state;
	int toState;
	FsObj::attr_t attr;

	ssql << "INSERT INTO JOB_QUEUE (OPERATION, REQ_NUM, TARGET_STATE, FILE_SIZE, "
		 << "FS_ID, I_GEN, I_NUM, MTIME, LAST_UPD, FILE_STATE, TAPE_ID, START_BLOCK, FAILED) "
		 << "VALUES (" << DataBase::TRARECALL << ", "            // OPERATION
		 << reqNum << ", "                                       // REQ_NUM
		 << ( recinfo.toresident ?
			  FsObj::RESIDENT : FsObj::PREMIGRATED ) << ", ";    // TARGET_STATE

	try {
		FsObj fso(recinfo.fsid, recinfo.igen, recinfo.ino);
		statbuf = fso.stat();

		if (!S_ISREG(statbuf.st_mode)) {
			MSG(LTFSDMS0032E, recinfo.ino);
			return;
		}

		ssql << statbuf.st_size << ", "                          // FILE_SIZE
			 << recinfo.fsid << ", "                             // FS_ID
			 << recinfo.igen << ", "                             // I_GEN
			 << recinfo.ino << ", "                              // I_NUM
			 << statbuf.st_mtime << ", "                         // MTIME
			 << time(NULL) << ", ";                              // LAST_UPD
		state = fso.getMigState();
		if ( fso.getMigState() == FsObj::RESIDENT ) {
			MSG(LTFSDMS0031I, recinfo.ino);
			return;
		}
		ssql << state << ", ";                                   // FILE_STATE
		attr = fso.getAttribute();
		ssql << "'" << attr.tapeId[0] << "', ";                  // TAPE_ID
		tapeName = Scheduler::getTapeName(recinfo.fsid, recinfo.igen,
										  recinfo.ino, tapeId);
		ssql << Scheduler::getStartBlock(tapeName) << ", "       // START_BLOCK
			 << 0 << ");";                                       // FAILED
	}
	catch ( int error ) {
		MSG(LTFSDMS0032E, recinfo.ino);
	}

	if ( recinfo.toresident )
		toState = FsObj::RESIDENT;
	else
		toState = FsObj::PREMIGRATED;


	sqlite3_statement::prepare(ssql.str(), &stmt);
	rc = sqlite3_statement::step(stmt);
	sqlite3_statement::checkRcAndFinalize(stmt, rc, SQLITE_DONE);

	if ( newReq == true ) {
		ssql.str("");
		ssql.clear();

		std::unique_lock<std::mutex> lock(Scheduler::mtx);

		ssql << "INSERT INTO REQUEST_QUEUE (OPERATION, REQ_NUM, "
			 << "TAPE_ID, TIME_ADDED, STATE) "
			 << "VALUES (" << DataBase::TRARECALL << ", "                          // OPERATION
			 << reqNum << ", "                                                     // REQ_NUMR
			 << "'" << attr.tapeId[0] << "', "                                     // TAPE_ID
			 << time(NULL) << ", "                                                 // TIME_ADDED
			 << DataBase::REQ_NEW << ");";                                         // STATE

		sqlite3_statement::prepare(ssql.str(), &stmt);
		rc = sqlite3_statement::step(stmt);
		sqlite3_statement::checkRcAndFinalize(stmt, rc, SQLITE_DONE);

		Scheduler::cond.notify_one();
	}
	else {
		bool reqCompleted = true;
		ssql.str("");
		ssql.clear();

		std::unique_lock<std::mutex> lock(Scheduler::mtx);

		ssql << "SELECT STATE FROM REQUEST_QUEUE WHERE REQ_NUM=" << reqNum;
		sqlite3_statement::prepare(ssql.str(), &stmt);
		while ( (rc = sqlite3_statement::step(stmt)) == SQLITE_ROW ) {
			if ( sqlite3_column_int(stmt, 0) != DataBase::REQ_COMPLETED ) {
				reqCompleted = false;
			}
		}
		sqlite3_statement::checkRcAndFinalize(stmt, rc, SQLITE_DONE);
		if ( reqCompleted == true ) {
			ssql.str("");
			ssql.clear();
			ssql << "UPDATE REQUEST_QUEUE SET STATE=" << DataBase::REQ_NEW
				 << " WHERE REQ_NUM=" << reqNum << " AND TAPE_ID='" << tapeId << "';";
			sqlite3_statement::prepare(ssql.str(), &stmt);
			rc = sqlite3_statement::step(stmt);
			sqlite3_statement::checkRcAndFinalize(stmt, rc, SQLITE_DONE);
			Scheduler::cond.notify_one();
		}
	}

	while ( true ) {
		bool reqCompleted = true;
		bool jobCompleted = true;
		ssql.str("");
		ssql.clear();

		std::unique_lock<std::mutex> lock(Scheduler::mtx);

		ssql << "SELECT STATE FROM REQUEST_QUEUE WHERE REQ_NUM=" << reqNum;
		sqlite3_statement::prepare(ssql.str(), &stmt);
		while ( (rc = sqlite3_statement::step(stmt)) == SQLITE_ROW ) {
			if ( sqlite3_column_int(stmt, 0) != DataBase::REQ_COMPLETED ) {
				reqCompleted = false;
			}
		}
		sqlite3_statement::checkRcAndFinalize(stmt, rc, SQLITE_DONE);

		ssql.str("");
		ssql.clear();

		ssql << "SELECT FILE_STATE FROM JOB_QUEUE WHERE"
			 << " FS_ID=" << recinfo.fsid
			 << " AND I_GEN=" << recinfo.igen
			 << " AND I_NUM=" << recinfo.ino
			 << " AND REQ_NUM=" << reqNum;
		sqlite3_statement::prepare(ssql.str(), &stmt);
		while ( (rc = sqlite3_statement::step(stmt)) == SQLITE_ROW ) {
			if ( sqlite3_column_int(stmt, 0) != toState ) {
				jobCompleted = false;
			}
		}
		sqlite3_statement::checkRcAndFinalize(stmt, rc, SQLITE_DONE);

		if  ( jobCompleted == true ) {
			ssql.str("");
			ssql.clear();

			ssql << "DELETE FROM JOB_QUEUE WHERE"
				 << " FS_ID=" << recinfo.fsid
				 << " AND I_GEN=" << recinfo.igen
				 << " AND I_NUM=" << recinfo.ino
				 << " AND REQ_NUM=" << reqNum;
			sqlite3_statement::prepare(ssql.str(), &stmt);
			rc = sqlite3_statement::step(stmt);
			sqlite3_statement::checkRcAndFinalize(stmt, rc, SQLITE_DONE);
			break;
		}
		else if ( reqCompleted == true ) {
			ssql.str("");
			ssql.clear();
			ssql << "UPDATE REQUEST_QUEUE SET STATE=" << DataBase::REQ_NEW
				 << " WHERE REQ_NUM=" << reqNum << " AND TAPE_ID='" << tapeId << "';";
			sqlite3_statement::prepare(ssql.str(), &stmt);
			rc = sqlite3_statement::step(stmt);
			sqlite3_statement::checkRcAndFinalize(stmt, rc, SQLITE_DONE);
			Scheduler::cond.notify_one();
		}
		lock.unlock();

		sleep(1);
	}

	Connector::respondRecallEvent(recinfo);
}

void TransRecall::run(Connector *connector)

{
	SubServer subs;
	Connector::rec_info_t recinfo;
	std::map<std::string, long> reqmap;
	bool newReq;

	try {
		connector->initTransRecalls();
	}
	catch ( int error ) {
		MSG(LTFSDMS0030E);
		return;
	}

	while (terminate == false) {
		recinfo = connector->getEvents();

		if ( recinfo.ino == 0 )
			continue;

		FsObj fso(recinfo.fsid, recinfo.igen, recinfo.ino);
		std::string tapeId = fso.getAttribute().tapeId[0];

		std::stringstream thrdinfo;
		thrdinfo << "TrRec(" << recinfo.ino << ")";

		if ( reqmap.count(tapeId) == 0 ) {
			reqmap[tapeId] = ++globalReqNumber;
			newReq = true;
		}
		else {
			newReq = false;
		}

		subs.enqueue(thrdinfo.str(), TransRecall::recall, recinfo, tapeId, reqmap[tapeId], newReq);
	}

	subs.waitAllRemaining();
}


unsigned long recall(unsigned long long fsid,unsigned int igen,	unsigned long long ino,
					 std::string tapeId, FsObj::file_state state, FsObj::file_state toState)

{
	struct stat statbuf;
	std::string tapeName;
	char buffer[Const::READ_BUFFER_SIZE];
	long rsize;
	long wsize;
	int fd = -1;
	long offset = 0;

	try {
		FsObj target(fsid, igen, ino);

		target.lock();

		if ( state == FsObj::MIGRATED ) {
			tapeName = Scheduler::getTapeName(fsid, igen, ino, tapeId);
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
					MSG(LTFSDMS0023E, tapeName.c_str());
					throw(errno);
				}
				wsize = target.write(offset, (unsigned long) rsize, buffer);
				if ( wsize != rsize ) {
					TRACE(Trace::error, errno);
					TRACE(Trace::error, wsize);
					TRACE(Trace::error, rsize);
					MSG(LTFSDMS0033E, ino);
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
	}

	return statbuf.st_size;
}


void recallStep(int reqNum, std::string tapeId)

{
	sqlite3_stmt *stmt;
	std::stringstream ssql;
	int rc, rc2;
	FsObj::file_state state;
	FsObj::file_state toState;
	unsigned long long fsid;
	unsigned int igen;
	unsigned long long ino;


	ssql << "SELECT FS_ID, I_GEN, I_NUM, FILE_STATE, TARGET_STATE  FROM JOB_QUEUE "
		 << "WHERE REQ_NUM=" << reqNum << " AND TAPE_ID='" << tapeId << "' ORDER BY START_BLOCK";

	sqlite3_statement::prepare(ssql.str(), &stmt);

	while ( (rc = sqlite3_statement::step(stmt) ) ) {
		if ( rc != SQLITE_ROW )
			break;

		fsid = (unsigned long long) sqlite3_column_int64(stmt, 0);
		igen = (unsigned int) sqlite3_column_int(stmt, 1);
		ino = (unsigned long long) sqlite3_column_int64(stmt, 2);
		state = (FsObj::file_state) sqlite3_column_int(stmt, 3);
		toState = (FsObj::file_state) sqlite3_column_int(stmt, 4);

		if ( state == FsObj::RESIDENT )
			continue;

		if ( state == toState )
			continue;

		recall(fsid, igen, ino, tapeId, state, toState);

		ssql.str("");
		ssql.clear();

		ssql << "UPDATE JOB_QUEUE SET FILE_STATE =" <<  toState << " WHERE"
			 << " FS_ID=" << fsid
			 << " AND I_GEN=" << igen
			 << " AND I_NUM=" << ino
			 << " AND REQ_NUM=" << reqNum;

		sqlite3_stmt *stmt2;

		sqlite3_statement::prepare(ssql.str(), &stmt2);
		rc2 = sqlite3_statement::step(stmt2);
		sqlite3_statement::checkRcAndFinalize(stmt2, rc2, SQLITE_DONE);
	}

	sqlite3_statement::checkRcAndFinalize(stmt, rc, SQLITE_DONE);
}


void TransRecall::execRequest(int reqNum, std::string tapeId)

{
	std::stringstream ssql;
	sqlite3_stmt *stmt;
	int rc;

	recallStep(reqNum, tapeId);

	std::unique_lock<std::mutex> lock(Scheduler::mtx);
	ssql << "UPDATE TAPE_LIST SET STATE=" << DataBase::TAPE_FREE
		 << " WHERE TAPE_ID='" << tapeId << "';";
	sqlite3_statement::prepare(ssql.str(), &stmt);
	rc = sqlite3_statement::step(stmt);
	sqlite3_statement::checkRcAndFinalize(stmt, rc, SQLITE_DONE);

	ssql.str("");
	ssql.clear();
	ssql << "UPDATE REQUEST_QUEUE SET STATE=" << DataBase::REQ_COMPLETED
		 << " WHERE REQ_NUM=" << reqNum << " AND TAPE_ID='" << tapeId << "';";
	sqlite3_statement::prepare(ssql.str(), &stmt);
	rc = sqlite3_statement::step(stmt);
	sqlite3_statement::checkRcAndFinalize(stmt, rc, SQLITE_DONE);

	Scheduler::updReq = reqNum;
	Scheduler::cond.notify_one();
}
