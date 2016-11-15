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

void Migration::addFileName(std::string fileName)

{
	int rc;
	struct stat statbuf;
	std::stringstream ssql;
	sqlite3_stmt *stmt;

	ssql << "INSERT INTO JOB_QUEUE (OPERATION, FILE_NAME, REQ_NUM, TARGET_STATE, "
		 << "COLOC_GRP, FILE_SIZE, FS_ID, I_GEN, I_NUM, MTIME, LAST_UPD, FILE_STATE, FAILED) "
		 << "VALUES (" << DataBase::MIGRATION << ", "            // OPERATION
		 << "'" << fileName << "', "                             // FILE_NAME
		 << reqNumber << ", "                                    // REQ_NUM
		 << targetState << ", "                                  // MIGRATION_STATE
		 << jobnum % colFactor << ", ";                          // COLOC_GRP

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
			 << time(NULL) << ", "                               // LAST_UPD
			 << fso.getMigState() << ", "                        // FILE_STATE
			 << 0 << ");";                                       // FAILED
	}
	catch ( int error ) {
		MSG(LTFSDMS0017E, fileName.c_str());
		return;
	}


	sqlite3_statement::prepare(ssql.str(), &stmt);

	rc = sqlite3_step(stmt);

	sqlite3_statement::checkRcAndFinalize(stmt, rc, SQLITE_DONE);

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

	while ( (rc = sqlite3_statement::step(stmt)) == SQLITE_ROW ) {
		std::unique_lock<std::mutex> lock(Scheduler::mtx);

		ssql2.str("");
		ssql2.clear();

		colNumber = sqlite3_column_int (stmt, 0);
		tapeId = tapeList[colNumber %tapeList.size()];

		ssql2 << "INSERT INTO REQUEST_QUEUE (OPERATION, REQ_NUM, TARGET_STATE, "
			  << "COLOC_GRP, TAPE_ID, TIME_ADDED, STATE) "
			  << "VALUES (" << DataBase::MIGRATION << ", "                         // OPERATION
			  << reqNumber << ", "                                                 // FILE_NAME
			  << targetState << ", "                                               // TARGET_STATE
			  << colNumber << ", "                                                 // COLOC_GRP
			  << "'" << tapeId << "', "                                            // TAPE_ID
			  << time(NULL) << ", "                                                // TIME_ADDED
			  << DataBase::REQ_NEW << ");";                                        // STATE

		sqlite3_statement::prepare(ssql2.str(), &stmt2);

		rc = sqlite3_statement::step(stmt2);

		sqlite3_statement::checkRcAndFinalize(stmt2, rc, SQLITE_DONE);

		Scheduler::cond.notify_one();
	}

	sqlite3_statement::checkRcAndFinalize(stmt, rc, SQLITE_DONE);
}


unsigned long preMigrate(std::string fileName, std::string tapeId)

{
	struct stat statbuf;
	std::string tapeName;
	char buffer[Const::READ_BUFFER_SIZE];
	long rsize;
	long wsize;
	int fd = -1;
	long offset = 0;


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

		statbuf = source.stat();

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
		}

		if ( fsetxattr(fd, Const::LTFS_ATTR.c_str(), fileName.c_str(), fileName.length(), 0) == -1 ) {
			TRACE(Trace::error, errno);
			MSG(LTFSDMS0025E, Const::LTFS_ATTR, tapeName);
			throw(errno);
		}

		close(fd);

		source.addAttribute(Const::DMAPI_ATTR, tapeId);
		source.finishPremigration();
		source.unlock();
	}
	catch ( int error ) {
		if ( fd != -1 )
			close(fd);
	}
	return statbuf.st_size;
}

void stub(std::string fileName)

{
	try {
		FsObj source(fileName);

		source.lock();
		source.prepareStubbing();
		source.stub();
		source.unlock();
	}
	catch ( int error ) {
	}
}


bool migrationStep(int reqNum, int colGrp, std::string tapeId, int fromState, int toState)

{
	sqlite3_stmt *stmt;
	std::stringstream ssql;
	int rc, rc2;
    int group_end = 0;
    int group_begin = -1;
	bool suspended = false;
	time_t start;
	std::unique_lock<std::mutex> lock(Scheduler::updmtx);

	Scheduler::updReq = reqNum;
	Scheduler::updcond.notify_all();
	lock.unlock();

	ssql << "SELECT ROWID, FILE_NAME FROM JOB_QUEUE WHERE REQ_NUM=" << reqNum
		 << " AND COLOC_GRP=" << colGrp << " AND FILE_STATE=" << fromState;

	sqlite3_statement::prepare(ssql.str(), &stmt);


	start = time(NULL);

	while ( (rc = sqlite3_statement::step(stmt) ) ) {
		if ( rc != SQLITE_ROW && rc != SQLITE_DONE )
			break;

		if ( rc == SQLITE_ROW ) {
			const char *cstr = reinterpret_cast<const char*>(sqlite3_column_text (stmt, 1));

			if (!cstr)
				continue;

			if ( toState == FsObj::PREMIGRATED ) {
				if ( Scheduler::suspend_map[tapeId] == true ) {
					suspended = true;
					Scheduler::suspend_map[tapeId] = false;
					sqlite3_statement::checkRcAndFinalize(stmt, rc, SQLITE_ROW);
					return suspended;
				}
				preMigrate(std::string(cstr), tapeId);
			}
			else {
				stub(std::string(cstr));
			}

			group_end = sqlite3_column_int(stmt, 0);
			if ( group_begin == -1 )
				group_begin = group_end;

			if ( time(NULL) - start < 10 )
				continue;
			start = time(NULL);
		}

		ssql.str("");
		ssql.clear();

		ssql << "UPDATE JOB_QUEUE SET FILE_STATE = " <<  toState
			 << " WHERE REQ_NUM=" << reqNum << " AND COLOC_GRP = " << colGrp
			 << " AND (ROWID BETWEEN " << group_begin << " AND " << group_end << ")";

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

	return suspended;
}

std::atomic<int> numPremig(0);

void Migration::execRequest(int reqNum, int tgtState, int colGrp, std::string tapeId)

{
	TRACE(Trace::much, __PRETTY_FUNCTION__);

	sqlite3_stmt *stmt;
	std::stringstream ssql;
	int rc;
	std::stringstream tapePath;
	bool suspended = false;

	numPremig++;
	assert(numPremig<3);

	suspended = migrationStep(reqNum, colGrp, tapeId,  FsObj::RESIDENT, FsObj::PREMIGRATED);

	tapePath << Const::LTFS_PATH << "/" << tapeId;

	if ( setxattr(tapePath.str().c_str(), Const::LTFS_SYNC_ATTR.c_str(),
				  Const::LTFS_SYNC_VAL.c_str(), Const::LTFS_SYNC_VAL.length(), 0) == -1 ) {
		if ( errno != ENODATA ) {
			TRACE(Trace::error, errno);
			MSG(LTFSDMS0024E, tapeId);
			throw(errno);
		}
	}

	std::unique_lock<std::mutex> lock(Scheduler::mtx);
	ssql << "UPDATE TAPE_LIST SET STATE=" << DataBase::TAPE_FREE
		 << " WHERE TAPE_ID='" << tapeId << "';";
	sqlite3_statement::prepare(ssql.str(), &stmt);
	rc = sqlite3_statement::step(stmt);
	sqlite3_statement::checkRcAndFinalize(stmt, rc, SQLITE_DONE);
	numPremig--;
	Scheduler::cond.notify_one();

	if ( tgtState != LTFSDmProtocol::LTFSDmMigRequest::PREMIGRATED )
		migrationStep(reqNum, colGrp, tapeId,  FsObj::PREMIGRATED, FsObj::MIGRATED);

	std::unique_lock<std::mutex> updlock(Scheduler::updmtx);

	ssql.str("");
	ssql.clear();
	if ( suspended )
		ssql << "UPDATE REQUEST_QUEUE SET STATE=" << DataBase::REQ_NEW;
	else
		ssql << "UPDATE REQUEST_QUEUE SET STATE=" << DataBase::REQ_COMPLETED;
	ssql << " WHERE REQ_NUM=" << reqNum << " AND COLOC_GRP=" << colGrp << ";";
	sqlite3_statement::prepare(ssql.str(), &stmt);
	rc = sqlite3_statement::step(stmt);
	sqlite3_statement::checkRcAndFinalize(stmt, rc, SQLITE_DONE);

	lock.unlock();

	Scheduler::updReq = reqNum;
	Scheduler::updcond.notify_one();
}
