#include <sys/types.h>
#include <sys/xattr.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <string>
#include <sstream>
#include <mutex>
#include <condition_variable>
#include <thread>
#include <typeinfo>
#include <map>

#include <sqlite3.h>
#include "src/common/comm/ltfsdm.pb.h"

#include "src/common/tracing/Trace.h"
#include "src/common/util/util.h"
#include "src/connector/Connector.h"
#include "SubServer.h"
#include "Server.h"
#include "DataBase.h"
#include "Scheduler.h"

std::mutex Scheduler::mtx;
std::condition_variable Scheduler::cond;
std::mutex Scheduler::updmtx;
std::condition_variable Scheduler::updcond;
std::atomic<int> Scheduler::updReq(Const::UNSET);

std::map<std::string, std::atomic<bool>> suspend_map;

std::string Scheduler::getTapeName(std::string fileName, std::string tapeId)

{
	FsObj diskFile(fileName);
	std::stringstream tapeName;

	tapeName << Const::LTFS_PATH
			 << "/" << tapeId << "/"
			 << Const::LTFS_NAME << "."
			 << diskFile.getFsId() << "."
			 << diskFile.getIGen() << "."
			 << diskFile.getINode();

	return tapeName.str();
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
	long accumSize = 0;
	long count = 0;
	long updateSize;
	std::unique_lock<std::mutex> lock(Scheduler::updmtx);

	Scheduler::updReq = reqNum;
	Scheduler::updcond.notify_all();
	lock.unlock();

	ssql << "SELECT ROWID, FILE_NAME FROM JOB_QUEUE WHERE REQ_NUM=" << reqNum;
	ssql << " AND COLOC_GRP=" << colGrp << " AND FILE_STATE=" << fromState;

	sqlite3_statement::prepare(ssql.str(), &stmt);


	while ( (rc = sqlite3_statement::step(stmt) ) ) {
		if ( rc != SQLITE_ROW && rc != SQLITE_DONE )
			break;

		if ( rc == SQLITE_ROW ) {
			const char *cstr = reinterpret_cast<const char*>(sqlite3_column_text (stmt, 1));

			if (!cstr)
				continue;

			if ( toState == FsObj::PREMIGRATED ) {
				if ( suspend_map[tapeId] == true ) {
					suspended = true;
					suspend_map[tapeId] = false;
					sqlite3_statement::checkRcAndFinalize(stmt, rc, SQLITE_ROW);
					return suspended;
				}
				accumSize += preMigrate(std::string(cstr), tapeId);
				count++;
			}
			else {
				stub(std::string(cstr));
			}

			group_end = sqlite3_column_int(stmt, 0);
			if ( group_begin == -1 )
				group_begin = group_end;
			updateSize = (8000*accumSize)/(count+1)+200000000L;
			if ( accumSize < (updateSize > 4000000000L ? 4000000000L : updateSize) )
				continue;
			accumSize = 0;
			count = 0;
		}

		ssql.str("");
		ssql.clear();

		ssql << "UPDATE JOB_QUEUE SET FILE_STATE = " <<  toState;
		ssql << " WHERE COLOC_GRP = " << colGrp << " AND (ROWID BETWEEN " << group_begin << " AND " << group_end << ")";

		sqlite3_stmt *stmt2;

		lock.lock();

		sqlite3_statement::prepare(ssql.str(), &stmt2);
		rc2 = sqlite3_statement::step(stmt2);
		sqlite3_statement::checkRcAndFinalize(stmt2, rc2, SQLITE_DONE);

		Scheduler::updReq = reqNum;
		Scheduler::updcond.notify_all();
		lock.unlock();

		if ( rc == SQLITE_DONE )
			break;
	}

	sqlite3_statement::checkRcAndFinalize(stmt, rc, SQLITE_DONE);

	return suspended;
}

std::atomic<int> numPremig(0);

void migration(int reqNum, int tgtState, int colGrp, std::string tapeId)

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

	if ( setxattr(tapePath.str().c_str(), Const::LTFS_SYNC_ATTR.c_str(), Const::LTFS_SYNC_VAL.c_str(), Const::LTFS_SYNC_VAL.length(), 0) == -1 ) {
		if ( errno != ENODATA ) {
			TRACE(Trace::error, errno);
			MSG(LTFSDMS0024E, tapeId);
			throw(errno);
		}
	}

	std::unique_lock<std::mutex> lock(Scheduler::mtx);
	ssql << "UPDATE TAPE_LIST SET STATE=" << DataBase::TAPE_FREE;
	ssql << " WHERE TAPE_ID='" << tapeId << "';";
	sqlite3_statement::prepare(ssql.str(), &stmt);
	rc = sqlite3_statement::step(stmt);
	sqlite3_statement::checkRcAndFinalize(stmt, rc, SQLITE_DONE);
	numPremig--;
	Scheduler::cond.notify_one();
	lock.unlock();

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

	Scheduler::updcond.notify_one();
}


unsigned long recall(std::string fileName, std::string tapeId, FsObj::file_state state, FsObj::file_state toState)

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

	ssql << "SELECT ROWID, FILE_NAME, FILE_STATE FROM JOB_QUEUE WHERE REQ_NUM=" << reqNum;
	ssql << " AND TAPE_ID='" << tapeId << "' ORDER BY START_BLOCK";

	sqlite3_statement::prepare(ssql.str(), &stmt);

	//recall(std::string fileName, std::string tapeId, int toState)

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

		ssql << "UPDATE JOB_QUEUE SET FILE_STATE = " <<  toState;
		ssql << " WHERE TAPE_ID ='" << tapeId << "' AND (ROWID BETWEEN ";
		ssql << group_begin << " AND " << group_end << ")";

		sqlite3_stmt *stmt2;

		lock.lock();

		sqlite3_statement::prepare(ssql.str(), &stmt2);
		rc2 = sqlite3_statement::step(stmt2);
		sqlite3_statement::checkRcAndFinalize(stmt2, rc2, SQLITE_DONE);

		Scheduler::updReq = reqNum;
		Scheduler::updcond.notify_all();
		lock.unlock();

		if ( rc == SQLITE_DONE )
			break;
	}

	sqlite3_statement::checkRcAndFinalize(stmt, rc, SQLITE_DONE);
}

void selrecall(int reqNum, int tgtState, std::string tapeId)

{
	std::stringstream ssql;
	sqlite3_stmt *stmt;
	int rc;

	if ( tgtState == LTFSDmProtocol::LTFSDmMigRequest::PREMIGRATED )
		recallStep(reqNum, tapeId, FsObj::PREMIGRATED);
	else
		recallStep(reqNum, tapeId, FsObj::RESIDENT);

	std::unique_lock<std::mutex> lock(Scheduler::mtx);
	ssql << "UPDATE TAPE_LIST SET STATE=" << DataBase::TAPE_FREE;
	ssql << " WHERE TAPE_ID='" << tapeId << "';";
	sqlite3_statement::prepare(ssql.str(), &stmt);
	rc = sqlite3_statement::step(stmt);
	sqlite3_statement::checkRcAndFinalize(stmt, rc, SQLITE_DONE);
	numPremig--;
	Scheduler::cond.notify_one();
	lock.unlock();

	std::unique_lock<std::mutex> updlock(Scheduler::updmtx);

	ssql.str("");
	ssql.clear();
	ssql << "UPDATE REQUEST_QUEUE SET STATE=" << DataBase::REQ_COMPLETED;
	ssql << " WHERE REQ_NUM=" << reqNum << " AND TAPE_ID='" << tapeId << "';";
	sqlite3_statement::prepare(ssql.str(), &stmt);
	rc = sqlite3_statement::step(stmt);
	sqlite3_statement::checkRcAndFinalize(stmt, rc, SQLITE_DONE);

	Scheduler::updcond.notify_one();
}

void Scheduler::run(long key)

{
	TRACE(Trace::much, __PRETTY_FUNCTION__);
	sqlite3_stmt *stmt;
	std::stringstream ssql;
	std::unique_lock<std::mutex> lock(mtx);
	SubServer subs;
	int rc;

	suspend_map["DV1480L6"] = false;
	suspend_map["DV1481L6"] = false;

	while (true) {
		cond.wait(lock);
		if(terminate == true) {
			break;
		}

		ssql.str("");
		ssql.clear();
		ssql << "SELECT OPERATION, REQ_NUM, TARGET_STATE, COLOC_GRP, TAPE_ID";
		ssql << " FROM REQUEST_QUEUE WHERE STATE=" << DataBase::REQ_NEW;
		ssql << " ORDER BY OPERATION,REQ_NUM;";

		sqlite3_statement::prepare(ssql.str(), &stmt);

		while ( (rc = sqlite3_statement::step(stmt)) == SQLITE_ROW ) {
			sqlite3_stmt *stmt2;

			const char *tape_id = reinterpret_cast<const char*>(sqlite3_column_text (stmt, 4));
			if (tape_id == NULL) {
				MSG(LTFSDMS0020E);
				continue;
			}
			std::string tapeId = std::string(tape_id);

			ssql.str("");
			ssql.clear();
			ssql << "SELECT STATE FROM TAPE_LIST WHERE TAPE_ID='" << tapeId << "';";

			sqlite3_statement::prepare(ssql.str(), &stmt2);
			while ( (rc = sqlite3_statement::step(stmt2)) == SQLITE_ROW ) {
				if ( sqlite3_column_int(stmt2, 0) != DataBase::TAPE_FREE ) {
					if ( sqlite3_column_int(stmt, 0) == DataBase::SELRECALL ) {
						suspend_map[tapeId] = true;
					}
					continue;
				}

				sqlite3_stmt *stmt3;

				ssql.str("");
				ssql.clear();
				ssql << "UPDATE TAPE_LIST SET STATE=" << DataBase::TAPE_INUSE;
				ssql << " WHERE TAPE_ID='" << tapeId << "';";
				sqlite3_statement::prepare(ssql.str(), &stmt3);
				rc = sqlite3_statement::step(stmt3);
				sqlite3_statement::checkRcAndFinalize(stmt3, rc, SQLITE_DONE);

				int reqNum = sqlite3_column_int(stmt, 1);
				int tgtState = sqlite3_column_int(stmt, 2);
				int colGrp = sqlite3_column_int(stmt, 3);


				std::stringstream thrdinfo;

				switch (sqlite3_column_int(stmt, 0)) {
					case DataBase::MIGRATION:
						ssql.str("");
						ssql.clear();
						ssql << "UPDATE REQUEST_QUEUE SET STATE=" << DataBase::REQ_INPROGRESS;
						ssql << " WHERE REQ_NUM=" << reqNum << " AND COLOC_GRP=" << colGrp << ";";
						sqlite3_statement::prepare(ssql.str(), &stmt3);
						rc = sqlite3_statement::step(stmt3);
						sqlite3_statement::checkRcAndFinalize(stmt3, rc, SQLITE_DONE);

						thrdinfo << "Migration(" << reqNum << "," << colGrp << ")";
						subs.enqueue(thrdinfo.str(), migration, reqNum, tgtState, colGrp, tapeId);
						break;
					case DataBase::SELRECALL:
						ssql.str("");
						ssql.clear();
						ssql << "UPDATE REQUEST_QUEUE SET STATE=" << DataBase::REQ_INPROGRESS;
						ssql << " WHERE REQ_NUM=" << reqNum << " AND TAPE_ID='" << tapeId << "';";
						sqlite3_statement::prepare(ssql.str(), &stmt3);
						rc = sqlite3_statement::step(stmt3);
						sqlite3_statement::checkRcAndFinalize(stmt3, rc, SQLITE_DONE);

						thrdinfo << "S.Recall(" << reqNum << ")";
						subs.enqueue(thrdinfo.str(), selrecall, reqNum, tgtState, tapeId);
						break;
					default:
						TRACE(Trace::error, sqlite3_column_int(stmt, 0));
				}
			}
			sqlite3_statement::checkRcAndFinalize(stmt2, rc, SQLITE_DONE);
		}
		sqlite3_statement::checkRcAndFinalize(stmt, rc, SQLITE_DONE);
	}
	subs.waitAllRemaining();
}
