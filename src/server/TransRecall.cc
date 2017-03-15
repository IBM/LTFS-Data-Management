#include <limits.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/resource.h>
#include <fcntl.h>

#include <map>
#include <set>
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


void TransRecall::addRequest(Connector::rec_info_t recinfo, std::string tapeId, long reqNum, bool newReq)

{
	int rc;
	struct stat statbuf;
	std::stringstream ssql;
	sqlite3_stmt *stmt;
	std::string tapeName;
	int state;
	FsObj::mig_attr_t attr;

	std::string filename;
	if ( recinfo.filename.compare("") == 0 )
		filename = "NULL";
	else
		filename = std::string("'") + recinfo.filename + std::string("'");

	ssql << "INSERT INTO JOB_QUEUE (OPERATION, FILE_NAME, REQ_NUM, TARGET_STATE, FILE_SIZE, FS_ID, I_GEN, "
		 << "I_NUM, MTIME_SEC, MTIME_NSEC, LAST_UPD, FILE_STATE, TAPE_ID, START_BLOCK, FILE_DESCRIPTOR, CONN_INFO) "
		 << "VALUES (" << DataBase::TRARECALL << ", "            // OPERATION
		 << filename.c_str() << ", "                  // FILE_NAME
		 << reqNum << ", "                                       // REQ_NUM
		 << ( recinfo.toresident ?
			  FsObj::RESIDENT : FsObj::PREMIGRATED ) << ", ";    // TARGET_STATE

	try {
		FsObj fso(recinfo);
		statbuf = fso.stat();

		if (!S_ISREG(statbuf.st_mode)) {
			MSG(LTFSDMS0032E, recinfo.ino);
			return;
		}

		ssql << statbuf.st_size << ", "                          // FILE_SIZE
			 << (long long) recinfo.fsid << ", "                 // FS_ID
			 << recinfo.igen << ", "                             // I_GEN
			 << (long long) recinfo.ino << ", "                  // I_NUM
			 << statbuf.st_mtime << ", "                         // MTIME_SEC
			 << 0 << ", "                                        // MTIME_NSEC
			 << time(NULL) << ", ";                              // LAST_UPD
		state = fso.getMigState();
		if ( fso.getMigState() == FsObj::RESIDENT ) {
			MSG(LTFSDMS0031I, recinfo.ino);
			return;
		}
		ssql << state << ", ";                                   // FILE_STATE
		attr = fso.getAttribute();
		ssql << "'" << tapeId << "', ";                          // TAPE_ID
		tapeName = Scheduler::getTapeName(recinfo.fsid, recinfo.igen,
										  recinfo.ino, tapeId);
		ssql << Scheduler::getStartBlock(tapeName) << ", ";      // START_BLOCK
		if ( recinfo.fd )
			ssql << recinfo.fd << ", ";                          // FILE_DESCRIPTOR
		else
			ssql << "NULL" << ", ";
		if (  recinfo.conn_info != NULL )
			ssql << (std::intptr_t) recinfo.conn_info << ");";   // CONN_INFO
		else
			ssql << "NULL" << ");";
	}
	catch ( int error ) {
		MSG(LTFSDMS0032E, recinfo.ino);
	}

	sqlite3_statement::prepare(ssql.str(), &stmt);
	rc = sqlite3_statement::step(stmt);
	sqlite3_statement::checkRcAndFinalize(stmt, rc, SQLITE_DONE);

	std::unique_lock<std::mutex> lock(Scheduler::mtx);

	if ( newReq == true ) {
		ssql.str("");
		ssql.clear();

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
}

void TransRecall::run(Connector *connector)

{
	SubServer subs(Const::MAX_TRANSPARENT_RECALL_THREADS);
	Connector::rec_info_t recinfo;
	std::map<std::string, long> reqmap;
	bool newReq;
	std::set<std::string> fsList;
	std::set<std::string>::iterator it;

	try {
		connector->initTransRecalls();
	}
	catch ( int error ) {
		MSG(LTFSDMS0030E);
		return;
	}

	fsList = LTFSDM::getFs();

	for ( it = fsList.begin(); it != fsList.end(); ++it ) {
		try {
			FsObj fileSystem(*it);
			if ( fileSystem.isFsManaged() ) {
				MSG(LTFSDMS0042I, *it);
				fileSystem.manageFs(true, connector->getStartTime());
			}
		}
		catch ( int error ) {
			switch ( error ) {
				case Error::LTFSDM_FS_CHECK_ERROR:
					MSG(LTFSDMS0044E, *it);
					break;
				case Error::LTFSDM_FS_ADD_ERROR:
					MSG(LTFSDMS0045E, *it);
					break;
				default:
					MSG(LTFSDMS0045E, *it);
			}
		}
	}

	while (terminate == false) {
		try {
			recinfo = connector->getEvents();
		}
		catch (int error ) {
			MSG(LTFSDMS0036W, error);
		}

		if ( recinfo.ino == 0 )
			continue;

		FsObj fso(recinfo);

		// error case: managed region set but no attrs
		try {
			if ( fso.getMigState() ==  FsObj::RESIDENT ) {
				fso.finishRecall(FsObj::RESIDENT);
				MSG(LTFSDMS0039I, recinfo.ino);
				connector->respondRecallEvent(recinfo, true);
				continue;
			}
		}
		catch ( int error ) {
			if ( error == Error::LTFSDM_ATTR_FORMAT )
				MSG(LTFSDMS0037W, recinfo.ino);
			else
				MSG(LTFSDMS0038W, recinfo.ino, error);
			connector->respondRecallEvent(recinfo, false);
			continue;
		}

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

		subs.enqueue(thrdinfo.str(), TransRecall::addRequest, recinfo, tapeId, reqmap[tapeId], newReq);
	}

	subs.waitAllRemaining();
}


unsigned long recall(Connector::rec_info_t recinfo, std::string tapeId,
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
		FsObj target(recinfo);

		target.lock();

		curstate = target.getMigState();

		if ( curstate != state ) {
			MSG(LTFSDMS0034I, recinfo.ino);
			state = curstate;
		}
		if ( state == FsObj::RESIDENT ) {
			return 0;
		}
		else if ( state == FsObj::MIGRATED ) {
			tapeName = Scheduler::getTapeName(recinfo.fsid, recinfo.igen, recinfo.ino, tapeId);
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
					MSG(LTFSDMS0033E, recinfo.ino);
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


void recallStep(int reqNum, std::string tapeId)

{
	Connector::rec_info_t recinfo;
	sqlite3_stmt *stmt;
	std::stringstream ssql;
	int rc, rc2;
	FsObj::file_state state;
	FsObj::file_state toState;
	int numFiles = 0;
	bool succeeded;

	ssql << "SELECT FS_ID, I_GEN, I_NUM, FILE_NAME, FILE_DESCRIPTOR, FILE_STATE, TARGET_STATE, CONN_INFO  FROM JOB_QUEUE "
		 << "WHERE REQ_NUM=" << reqNum << " AND TAPE_ID='" << tapeId << "' ORDER BY START_BLOCK";

	sqlite3_statement::prepare(ssql.str(), &stmt);

	DB.beginTransaction();

	while ( (rc = sqlite3_statement::step(stmt) ) ) {
		if ( rc != SQLITE_ROW )
			break;

		numFiles++;

		recinfo = (Connector::rec_info_t) {0, 0, 0, 0, 0, 0, ""};
		recinfo.fsid = (unsigned long long) sqlite3_column_int64(stmt, 0);
		recinfo.igen = (unsigned int) sqlite3_column_int(stmt, 1);
		recinfo.ino = (unsigned long long) sqlite3_column_int64(stmt, 2);
		const char *cstr = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));
		if ( cstr != NULL )
			recinfo.filename = std::string(cstr);
		recinfo.fd =  (int) sqlite3_column_int(stmt, 4);
		state = (FsObj::file_state) sqlite3_column_int(stmt, 5);
		toState = (FsObj::file_state) sqlite3_column_int(stmt, 6);
		if ( toState == FsObj::RESIDENT )
			recinfo.toresident = true;
		recinfo.conn_info = (struct conn_info_t *) sqlite3_column_int64(stmt, 7);

		try {
			recall(recinfo, tapeId, state, toState);
			succeeded = true;
		}
		catch(int error) {
			succeeded = false;
		}

		ssql.str("");
		ssql.clear();

		ssql << "DELETE FROM JOB_QUEUE WHERE"
			 << " FS_ID=" << (long long) recinfo.fsid
			 << " AND I_GEN=" << recinfo.igen
			 << " AND I_NUM=" << (long long) recinfo.ino
			 << " AND REQ_NUM=" << reqNum;

		sqlite3_stmt *stmt2;

		sqlite3_statement::prepare(ssql.str(), &stmt2);
		rc2 = sqlite3_statement::step(stmt2);
		sqlite3_statement::checkRcAndFinalize(stmt2, rc2, SQLITE_DONE);

		Connector::respondRecallEvent(recinfo, succeeded);
	}

	DB.endTransaction();

	TRACE(Trace::medium, numFiles);

	sqlite3_statement::checkRcAndFinalize(stmt, rc, SQLITE_DONE);
}


void TransRecall::execRequest(int reqNum, std::string tapeId)

{
	std::stringstream ssql;
	sqlite3_stmt *stmt;
	int remaining = 0;
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
	ssql << "SELECT FILE_STATE, COUNT(*) FROM JOB_QUEUE WHERE REQ_NUM="
		 << reqNum << " AND TAPE_ID='" << tapeId << "';";
	sqlite3_statement::prepare(ssql.str(), &stmt);
	while ( (rc = sqlite3_statement::step(stmt)) == SQLITE_ROW )
				remaining = sqlite3_column_int(stmt, 1);
	sqlite3_statement::checkRcAndFinalize(stmt, rc, SQLITE_DONE);

	ssql.str("");
	ssql.clear();
	ssql << "UPDATE REQUEST_QUEUE SET STATE=" << (remaining ? DataBase::REQ_NEW : DataBase::REQ_COMPLETED )
		 << " WHERE REQ_NUM=" << reqNum << " AND TAPE_ID='" << tapeId << "';";
	sqlite3_statement::prepare(ssql.str(), &stmt);
	rc = sqlite3_statement::step(stmt);
	sqlite3_statement::checkRcAndFinalize(stmt, rc, SQLITE_DONE);
	Scheduler::cond.notify_one();
}
