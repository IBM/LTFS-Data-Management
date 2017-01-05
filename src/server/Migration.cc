#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/xattr.h>
#include <unistd.h>

#include <iostream>
#include <sstream>
#include <vector>

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
#include "Migration.h"

void Migration::addJob(std::string fileName)

{
	int rc;
	struct stat statbuf;
	std::stringstream ssql;
	sqlite3_stmt *stmt;

	ssql.str("");
	ssql.clear();
	ssql << "INSERT INTO JOB_QUEUE (OPERATION, FILE_NAME, REQ_NUM, TARGET_STATE, REPL_NUM, COLOC_GRP, "
		 << "FILE_SIZE, FS_ID, I_GEN, I_NUM, MTIME_SEC, MTIME_NSEC, LAST_UPD, FILE_STATE) "
		 << "VALUES (" << DataBase::MIGRATION << ", "            // OPERATION
		 << "'" << fileName << "', "                             // FILE_NAME
		 << reqNumber << ", "                                    // REQ_NUM
		 << targetState << ", "                                  // MIGRATION_STATE
		 << "?, "                                                // REPL_NUM
		 << jobnum % colFactor << ", ";                          // COLOC_GRP

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
			 << time(NULL) << ", "                               // LAST_UPD
			 << fso.getMigState() << ");";                       // FILE_STATE
	}
	catch ( int error ) {
		ssql.str("");
		ssql.clear();
		ssql << "INSERT INTO JOB_QUEUE (OPERATION, FILE_NAME, REQ_NUM, TARGET_STATE, REPL_NUM, COLOC_GRP, "
			 << "FILE_SIZE, FS_ID, I_GEN, I_NUM, MTIME_SEC, MTIME_NSEC, LAST_UPD, FILE_STATE) "
			 << "VALUES (" << DataBase::MIGRATION << ", "        // OPERATION
			 << "'" << fileName << "', "                         // FILE_NAME
			 << reqNumber << ", "                                // REQ_NUM
			 << targetState << ", "                              // MIGRATION_STATE
			 << "?, "                                            // REPL_NUM
			 << jobnum % colFactor << ", "                       // COLOC_GRP
			 << Const::UNSET  << ", "                            // FILE_SIZE
			 << Const::UNSET  << ", "                            // FS_ID
			 << Const::UNSET  << ", "                            // I_GEN
			 << Const::UNSET  << ", "                            // I_NUM
			 << Const::UNSET  << ", "                            // MTIME_SEC
			 << Const::UNSET  << ", "                            // MTIME_NSEC
			 << time(NULL) << ", "                               // LAST_UPD
			 << FsObj::FAILED  << ");";                          // FILE_STATE
		MSG(LTFSDMS0017E, fileName.c_str());
	}

	for ( int replNum = 0; replNum < numReplica; replNum++ ) {
		TRACE(Trace::much, ssql.str());
		sqlite3_statement::prepare(ssql.str(), &stmt);
		if ( (rc = sqlite3_bind_int(stmt, 1, replNum)) != SQLITE_OK ) {
			TRACE(Trace::error, rc);
			MSG(LTFSDMS0028E, fileName.c_str());
			return;
		}
		rc = sqlite3_step(stmt);
		sqlite3_statement::checkRcAndFinalize(stmt, rc, SQLITE_DONE);
	}

	jobnum++;

	return;
}

std::vector<std::string> Migration::getTapes()

{
	int rc;
	std::string sql;
	sqlite3_stmt *stmt;

	sql = std::string("SELECT * FROM TAPE_LIST");

	sqlite3_statement::prepare(sql, &stmt);

	while ( (rc = sqlite3_statement::step(stmt)) == SQLITE_ROW ) {
		tapeList.push_back(reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0)));
	}

	sqlite3_statement::checkRcAndFinalize(stmt, rc, SQLITE_DONE);

	return tapeList;
}

void Migration::addRequest()

{
	int rc;
	std::stringstream ssql;
	sqlite3_stmt *stmt;
	int colNumber;
	std::string tapeId;

	ssql << "SELECT COLOC_GRP FROM JOB_QUEUE WHERE REQ_NUM="
		 << reqNumber << " GROUP BY COLOC_GRP";

	sqlite3_statement::prepare(ssql.str(), &stmt);

	std::stringstream ssql2;
	sqlite3_stmt *stmt2;

	getTapes();

	TRACE(Trace::little, reqNumber);
	TRACE(Trace::little, numReplica);
	TRACE(Trace::little, colFactor);

	Scheduler::updReq[reqNumber] = false;

	while ( (rc = sqlite3_statement::step(stmt)) == SQLITE_ROW ) {
		colNumber = sqlite3_column_int (stmt, 0);

		for (int replNum=0; replNum < numReplica; replNum++) {
			std::unique_lock<std::mutex> lock(Scheduler::mtx);

			ssql2.str("");
			ssql2.clear();

			tapeId = tapeList[(colNumber + replNum) % tapeList.size()];

			ssql2 << "INSERT INTO REQUEST_QUEUE (OPERATION, REQ_NUM, TARGET_STATE, "
				  << "NUM_REPL, REPL_NUM, COLOC_GRP, TAPE_ID, TIME_ADDED, STATE) "
				  << "VALUES (" << DataBase::MIGRATION << ", "                         // OPERATION
				  << reqNumber << ", "                                                 // FILE_NAME
				  << targetState << ", "                                               // TARGET_STATE
				  << numReplica << ", "                                                // NUM_REPL
				  << replNum << ", "                                                   // REPL_NUM
				  << colNumber << ", "                                                 // COLOC_GRP
				  << "'" << tapeId << "', "                                            // TAPE_ID
				  << time(NULL) << ", "                                                // TIME_ADDED
				  << DataBase::REQ_NEW << ");";                                        // STATE

			sqlite3_statement::prepare(ssql2.str(), &stmt2);

			rc = sqlite3_statement::step(stmt2);

			sqlite3_statement::checkRcAndFinalize(stmt2, rc, SQLITE_DONE);

			Scheduler::cond.notify_one();
		}
	}

	sqlite3_statement::checkRcAndFinalize(stmt, rc, SQLITE_DONE);
}


unsigned long preMigrate(std::string fileName, std::string tapeId, long secs, long nsecs)

{
	struct stat statbuf;
	std::string tapeName;
	char buffer[Const::READ_BUFFER_SIZE];
	long rsize;
	long wsize;
	int fd = -1;
	long offset = 0;
	FsObj::mig_attr_t attr;

	try {
		FsObj source(fileName);

		tapeName = Scheduler::getTapeName(fileName, tapeId);

		fd = open(tapeName.c_str(), O_RDWR | O_CREAT);

		if ( fd == -1 ) {
			TRACE(Trace::error, errno);
			MSG(LTFSDMS0021E, tapeName.c_str());
			throw(Error::LTFSDM_GENERAL_ERROR);
		}

		source.lock();

		source.preparePremigration();

		if ( stat(fileName.c_str(), &statbuf) == -1 ) {
			TRACE(Trace::error, errno);
			MSG(LTFSDMS0040E, fileName);
			throw(Error::LTFSDM_GENERAL_ERROR);
		}
		if ( statbuf.st_mtim.tv_sec != secs || statbuf.st_mtim.tv_nsec != nsecs ) {
			TRACE(Trace::error, statbuf.st_mtim.tv_sec);
			TRACE(Trace::error, secs);
			TRACE(Trace::error, statbuf.st_mtim.tv_nsec);
			TRACE(Trace::error, nsecs);
			MSG(LTFSDMS0041W, fileName);
			throw(Error::LTFSDM_GENERAL_ERROR);
		}

		while ( offset < statbuf.st_size ) {
			rsize = source.read(offset, statbuf.st_size - offset > Const::READ_BUFFER_SIZE ?
								Const::READ_BUFFER_SIZE : statbuf.st_size - offset , buffer);
			if ( rsize == -1 ) {
				TRACE(Trace::error, errno);
				MSG(LTFSDMS0023E, fileName);
				throw(errno);
			}
			wsize = write(fd, buffer, rsize);
			if ( wsize != rsize ) {
				TRACE(Trace::error, errno);
				TRACE(Trace::error, wsize);
				TRACE(Trace::error, rsize);
				MSG(LTFSDMS0022E, tapeName.c_str());
				close(fd);
				throw(Error::LTFSDM_GENERAL_ERROR);
			}
			offset += rsize;
			if ( stat(fileName.c_str(), &statbuf) == -1 ) {
				TRACE(Trace::error, errno);
				MSG(LTFSDMS0040E, fileName);
				throw(Error::LTFSDM_GENERAL_ERROR);
			}
			if ( statbuf.st_mtim.tv_sec != secs || statbuf.st_mtim.tv_nsec != nsecs ) {
				TRACE(Trace::error, statbuf.st_mtim.tv_sec);
				TRACE(Trace::error, secs);
				TRACE(Trace::error, statbuf.st_mtim.tv_nsec);
				TRACE(Trace::error, nsecs);
				MSG(LTFSDMS0041W, fileName);
				throw(Error::LTFSDM_GENERAL_ERROR);
			}
		}

		if ( fsetxattr(fd, Const::LTFS_ATTR.c_str(), fileName.c_str(), fileName.length(), 0) == -1 ) {
			TRACE(Trace::error, errno);
			MSG(LTFSDMS0025E, Const::LTFS_ATTR, tapeName);
			throw(errno);
		}

		close(fd);

		attr = source.getAttribute();
		memset(attr.tapeId[attr.copies], 0, Const::tapeIdLength+1);
		strncpy(attr.tapeId[attr.copies], tapeId.c_str(), Const::tapeIdLength);
		attr.copies++;
		source.addAttribute(attr);
		source.unlock();
	}
	catch ( int error ) {
		if ( fd != -1 )
			close(fd);
		throw(error);
	}
	return statbuf.st_size;
}

void stub(std::string fileName, int numRepl)

{
	try {
		FsObj source(fileName);
		FsObj::mig_attr_t attr;

		source.lock();
		attr = source.getAttribute();
		if ( attr.copies == numRepl ) {
			source.prepareStubbing();
			source.stub();
		}
		source.unlock();
	}
	catch ( int error ) {
	}
}


bool migrationStep(int reqNum, int numRepl, int replNum, int colGrp, std::string tapeId, int fromState, int toState)

{
	sqlite3_stmt *stmt;
	std::stringstream ssql;
	int rc, rc2;
    int group_end = 0;
    int group_begin = -1;
	bool suspended = false;
	time_t start;
	long secs;
	long nsecs;


	std::unique_lock<std::mutex> lock(Scheduler::updmtx);
	TRACE(Trace::much, reqNum);
	Scheduler::updReq[reqNum] = true;
	Scheduler::updcond.notify_all();
	lock.unlock();

	ssql << "SELECT ROWID, FILE_NAME, MTIME_SEC, MTIME_NSEC FROM JOB_QUEUE WHERE"
		 << " REQ_NUM=" << reqNum
		 << " AND COLOC_GRP=" << colGrp
		 << " AND FILE_STATE=" << fromState
		 << " AND REPL_NUM=" << replNum;

	sqlite3_statement::prepare(ssql.str(), &stmt);


	start = time(NULL);

	while ( (rc = sqlite3_statement::step(stmt) ) ) {
		if ( rc != SQLITE_ROW && rc != SQLITE_DONE )
			break;

		if ( rc == SQLITE_ROW ) {
			const char *cstr = reinterpret_cast<const char*>(sqlite3_column_text (stmt, 1));
			secs = sqlite3_column_int64(stmt, 2);
			nsecs = sqlite3_column_int64(stmt, 3);

			if (!cstr)
				continue;

			try {
				if ( toState == FsObj::PREMIGRATED ) {
					if ( Scheduler::suspend_map[tapeId] == true ) {
						suspended = true;
						Scheduler::suspend_map[tapeId] = false;
						sqlite3_statement::checkRcAndFinalize(stmt, rc, SQLITE_ROW);
						return suspended;
					}
					preMigrate(std::string(cstr), tapeId, secs, nsecs);
				}
				else {
					stub(std::string(cstr), numRepl);
				}
				group_end = sqlite3_column_int(stmt, 0);
			}
			catch(int error) {
				ssql.str("");
				ssql.clear();
				ssql << "UPDATE JOB_QUEUE SET FILE_STATE = " <<  FsObj::FAILED
					 << " WHERE REQ_NUM=" << reqNum
					 << " AND COLOC_GRP = " << colGrp
					 << " AND REPL_NUM=" << replNum
					 << " AND ROWID=" << sqlite3_column_int(stmt, 0);;
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
			 << " AND COLOC_GRP = " << colGrp
			 << " AND FILE_STATE=" << fromState
			 << " AND REPL_NUM=" << replNum
			 << " AND (ROWID BETWEEN " << group_begin << " AND " << group_end << ")";
		TRACE(Trace::much, ssql.str());

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

	return suspended;
}

std::atomic<int> numPremig(0);

void Migration::execRequest(int reqNum, int tgtState, int numRepl, int replNum, int colGrp, std::string tapeId)

{
	TRACE(Trace::much, __PRETTY_FUNCTION__);

	sqlite3_stmt *stmt;
	std::stringstream ssql;
	int rc;
	std::stringstream tapePath;
	bool suspended = false;
	bool failed = false;

	numPremig++;
	assert(numPremig<3);

	suspended = migrationStep(reqNum, numRepl, replNum, colGrp, tapeId,  FsObj::RESIDENT, FsObj::PREMIGRATED);

	tapePath << Const::LTFS_PATH << "/" << tapeId;

	if ( setxattr(tapePath.str().c_str(), Const::LTFS_SYNC_ATTR.c_str(),
				  Const::LTFS_SYNC_VAL.c_str(), Const::LTFS_SYNC_VAL.length(), 0) == -1 ) {
		if ( errno != ENODATA ) {
			TRACE(Trace::error, errno);
			MSG(LTFSDMS0024E, tapeId);

			ssql.str("");
			ssql.clear();

			ssql << "UPDATE JOB_QUEUE SET FILE_STATE = " <<  FsObj::FAILED
				 << " WHERE REQ_NUM=" << reqNum
				 << " AND COLOC_GRP = " << colGrp
				 << " AND FILE_STATE=" << FsObj::PREMIGRATED
				 << " AND REPL_NUM=" << replNum;

			TRACE(Trace::error, ssql.str());

			sqlite3_statement::prepare(ssql.str(), &stmt);
			rc = sqlite3_statement::step(stmt);
			sqlite3_statement::checkRcAndFinalize(stmt, rc, SQLITE_DONE);

			std::unique_lock<std::mutex> lock(Scheduler::updmtx);
			TRACE(Trace::error, reqNum);
			Scheduler::updReq[reqNum] = true;
			Scheduler::updcond.notify_all();

			failed = true;
		}
	}

	ssql.str("");
	ssql.clear();

	std::unique_lock<std::mutex> lock(Scheduler::mtx);
	ssql << "UPDATE TAPE_LIST SET STATE=" << DataBase::TAPE_FREE
		 << " WHERE TAPE_ID='" << tapeId << "';";
	sqlite3_statement::prepare(ssql.str(), &stmt);
	rc = sqlite3_statement::step(stmt);
	sqlite3_statement::checkRcAndFinalize(stmt, rc, SQLITE_DONE);
	numPremig--;
	Scheduler::cond.notify_one();

	if ( !failed && tgtState != LTFSDmProtocol::LTFSDmMigRequest::PREMIGRATED )
		migrationStep(reqNum, numRepl, replNum, colGrp, tapeId,  FsObj::PREMIGRATED, FsObj::MIGRATED);

	std::unique_lock<std::mutex> updlock(Scheduler::updmtx);

	ssql.str("");
	ssql.clear();
	if ( suspended )
		ssql << "UPDATE REQUEST_QUEUE SET STATE=" << DataBase::REQ_NEW;
	else
		ssql << "UPDATE REQUEST_QUEUE SET STATE=" << DataBase::REQ_COMPLETED;
	ssql << " WHERE REQ_NUM=" << reqNum
		 << " AND COLOC_GRP=" << colGrp
		 << " AND REPL_NUM=" << replNum << ";";

	sqlite3_statement::prepare(ssql.str(), &stmt);
	rc = sqlite3_statement::step(stmt);
	sqlite3_statement::checkRcAndFinalize(stmt, rc, SQLITE_DONE);

	lock.unlock();

	Scheduler::updReq[reqNum] = true;
	Scheduler::updcond.notify_all();
}
