#include "ServerIncludes.h"


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
		 << "I_NUM, MTIME_SEC, MTIME_NSEC, LAST_UPD, FILE_STATE, TAPE_ID, START_BLOCK) "
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
		if ( state == FsObj::RESIDENT ) {
			MSG(LTFSDMS0026I, fileName.c_str());
			return;
		}
		ssql << state << ", ";                                   // FILE_STATE
		attr = fso.getAttribute();
		ssql << "'" << attr.tapeId[0] << "', ";                  // TAPE_ID
		if ( state == FsObj::MIGRATED ) {
			needsTape.insert(attr.tapeId[0]);
		}
		tapeName = Scheduler::getTapeName(fileName, attr.tapeId[0]);
		ssql << Scheduler::getStartBlock(tapeName) << ");";      // START_BLOCK
	}
	catch ( const std::exception& e ) {
		TRACE(Trace::error, e.what());
		ssql.str("");
		ssql.clear();
		ssql << "INSERT INTO JOB_QUEUE (OPERATION, FILE_NAME, REQ_NUM, TARGET_STATE, FILE_SIZE, FS_ID, I_GEN, "
			 << "I_NUM, MTIME_SEC, MTIME_NSEC, LAST_UPD, FILE_STATE, TAPE_ID) "
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
			 << "'" << Const::FAILED_TAPE_ID << "'" << ");";     // TAPE_ID
		MSG(LTFSDMS0017E, fileName.c_str());
	}

	TRACE(Trace::normal, ssql.str());

	sqlite3_statement::prepare(ssql.str(), &stmt);

	rc = sqlite3_statement::step(stmt);

	sqlite3_statement::checkRcAndFinalize(stmt, rc, SQLITE_DONE);

	TRACE(Trace::always, fileName);
	TRACE(Trace::always, attr.tapeId[0]);

	return;
}

void SelRecall::addRequest()

{
	int rc;
	std::stringstream ssql;
	sqlite3_stmt *stmt;
	std::stringstream thrdinfo;
	SubServer subs;

	ssql.str("");
	ssql.clear();

	ssql << "SELECT TAPE_ID FROM JOB_QUEUE WHERE REQ_NUM="
		 << reqNumber << " GROUP BY TAPE_ID";

	sqlite3_statement::prepare(ssql.str(), &stmt);

	{
		std::lock_guard<std::mutex> updlock(Scheduler::updmtx);
		Scheduler::updReq[reqNumber] = false;
	}

	while ( (rc = sqlite3_statement::step(stmt)) == SQLITE_ROW ) {
		std::unique_lock<std::mutex> lock(Scheduler::mtx);

		std::stringstream ssql2;
		sqlite3_stmt *stmt2;

		const char *cstr = reinterpret_cast<const char*>(sqlite3_column_text (stmt, 0));
		std::string tapeId = std::string(cstr ? cstr : "");

		ssql2 << "INSERT INTO REQUEST_QUEUE (OPERATION, REQ_NUM, TARGET_STATE, "
			  << "TAPE_ID, TIME_ADDED, STATE) "
			  << "VALUES (" << DataBase::SELRECALL << ", "                          // OPERATION
			  << reqNumber << ", "                                                  // REQ_NUM
			  << targetState << ", "                                                // TARGET_STATE
			  << "'" << tapeId << "', "                                             // TAPE_ID
			  << time(NULL) << ", ";                                                // TIME_ADDED
		if ( tapeId.compare(Const::FAILED_TAPE_ID) == 0 )
			ssql2 << DataBase::REQ_COMPLETED << ");";                               // STATE
		else if ( needsTape.count(tapeId) > 0 )
			ssql2 << DataBase::REQ_NEW << ");";                                     // STATE
		else
			ssql2 << DataBase::REQ_INPROGRESS << ");";                              // STATE

		TRACE(Trace::normal, ssql2.str());

		sqlite3_statement::prepare(ssql2.str(), &stmt2);

		rc = sqlite3_statement::step(stmt2);

		sqlite3_statement::checkRcAndFinalize(stmt2, rc, SQLITE_DONE);

		TRACE(Trace::always, needsTape.count(tapeId));
		TRACE(Trace::always, reqNumber);
		TRACE(Trace::always, tapeId);

		if ( needsTape.count(tapeId) > 0 ) {
			Scheduler::cond.notify_one();
		}
		else {
			thrdinfo << "SelRec(" << reqNumber << ")";
			subs.enqueue(thrdinfo.str(), SelRecall::execRequest, reqNumber, targetState, tapeId, false);
		}
	}

	sqlite3_statement::checkRcAndFinalize(stmt, rc, SQLITE_DONE);

	subs.waitAllRemaining();
}


unsigned long SelRecall::recall(std::string fileName, std::string tapeId,
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

		TRACE(Trace::always, fileName);

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
				throw(EXCEPTION(Const::UNSET, tapeName, errno));
			}

			statbuf = target.stat();

			target.prepareRecall();

			while ( offset < statbuf.st_size ) {
				if ( Server::forcedTerminate )
					throw(EXCEPTION(Error::LTFSDM_OK));

				rsize = read(fd, buffer, sizeof(buffer));
				if ( rsize == -1 ) {
					TRACE(Trace::error, errno);
					MSG(LTFSDMS0023E,  tapeName.c_str());
					throw(EXCEPTION(Const::UNSET, fileName, errno));
				}
				wsize = target.write(offset, (unsigned long) rsize, buffer);
				if ( wsize != rsize ) {
					TRACE(Trace::error, errno);
					TRACE(Trace::error, wsize);
					TRACE(Trace::error, rsize);
					MSG(LTFSDMS0027E, fileName.c_str());
					close(fd);
					throw(EXCEPTION(Const::UNSET, fileName, wsize, rsize));
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
	catch ( const std::exception& e ) {
		if ( fd != -1 )
			close(fd);
		TRACE(Trace::error, e.what());
		throw(EXCEPTION(Const::UNSET));
	}

	return statbuf.st_size;
}

bool SelRecall::recallStep(int reqNumber, std::string tapeId, FsObj::file_state toState, bool needsTape)

{
	sqlite3_stmt *stmt;
	std::stringstream ssql;
	std::shared_ptr<OpenLTFSDrive> drive = nullptr;
	std::list<unsigned long> inumList;
	bool suspended = false;
	int rc, rc2;
	time_t start;

	TRACE(Trace::full, reqNumber);

	{
		std::lock_guard<std::mutex> lock(Scheduler::updmtx);
		Scheduler::updReq[reqNumber] = true;
		Scheduler::updcond.notify_all();
	}

	if ( needsTape ) {
		for ( std::shared_ptr<OpenLTFSDrive> d : inventory->getDrives() ) {
			if ( d->get_slot() == inventory->getCartridge(tapeId)->get_slot() ) {
				drive = d;
				break;
			}
		}
		assert(drive != nullptr);
	}

	ssql << "UPDATE JOB_QUEUE SET FILE_STATE=" <<  FsObj::RECALLING_MIG
		 << " WHERE REQ_NUM=" << reqNumber
		 << " AND FILE_STATE=" << FsObj::MIGRATED
		 << " AND TAPE_ID='" << tapeId << "'";

	TRACE(Trace::normal, ssql.str());

	sqlite3_statement::prepare(ssql.str(), &stmt);
	rc2 = sqlite3_statement::step(stmt);
	sqlite3_statement::checkRcAndFinalize(stmt, rc2, SQLITE_DONE);

	ssql.str("");
	ssql.clear();

	ssql << "UPDATE JOB_QUEUE SET FILE_STATE=" <<  FsObj::RECALLING_PREMIG
		 << " WHERE REQ_NUM=" << reqNumber
		 << " AND FILE_STATE=" << FsObj::PREMIGRATED
		 << " AND TAPE_ID='" << tapeId << "'";

	TRACE(Trace::normal, ssql.str());

	sqlite3_statement::prepare(ssql.str(), &stmt);
	rc2 = sqlite3_statement::step(stmt);
	sqlite3_statement::checkRcAndFinalize(stmt, rc2, SQLITE_DONE);

	ssql.str("");
	ssql.clear();

	ssql << "SELECT FILE_NAME, FILE_STATE, I_NUM FROM JOB_QUEUE WHERE REQ_NUM=" << reqNumber
		 << " AND TAPE_ID='" << tapeId << "'"
		 << " AND (FILE_STATE=" << FsObj::RECALLING_MIG << " OR FILE_STATE=" << FsObj::RECALLING_PREMIG << ")"
		 << " ORDER BY START_BLOCK";

	TRACE(Trace::normal, ssql.str());

	sqlite3_statement::prepare(ssql.str(), &stmt);

	start = time(NULL);

	while ( (rc = sqlite3_statement::step(stmt) ) ) {
		if ( Server::terminate == true ) {
			rc = SQLITE_DONE;
			break;
		}

		if ( rc != SQLITE_ROW && rc != SQLITE_DONE )
			break;

		if ( rc == SQLITE_ROW ) {
			std::string fileName;
			const char *cstr = reinterpret_cast<const char*>(sqlite3_column_text (stmt, 0));

			if (!cstr)
				continue;
			else
				fileName = std::string(cstr);

			FsObj::file_state state = (FsObj::file_state) sqlite3_column_int(stmt, 1);
			unsigned long inum = static_cast<unsigned long>(sqlite3_column_int64(stmt, 2));

			if ( state ==  FsObj::RECALLING_MIG )
				state = FsObj::MIGRATED;
			else
				state = FsObj::PREMIGRATED;

			TRACE(Trace::always, fileName);
			TRACE(Trace::always, state);
			TRACE(Trace::always, toState);

			if ( state == toState )
				continue;

			if ( needsTape && drive->getToUnblock() == DataBase::TRARECALL ) {
				suspended = true;
				rc = SQLITE_DONE;
				break;
			}

			try {
				if ( (state == FsObj::MIGRATED) && (needsTape == false) ) {
					MSG(LTFSDMS0047E, fileName);
					throw(EXCEPTION(Const::UNSET, fileName));
				}
				recall(fileName, tapeId, state, toState);
				inumList.push_back(inum);
				mrStatus.updateSuccess(reqNumber, state, toState);
			}
			catch (const std::exception& e) {
				TRACE(Trace::error, e.what());
				mrStatus.updateFailed(reqNumber, state);
				ssql.str("");
				ssql.clear();
				ssql << "UPDATE JOB_QUEUE SET FILE_STATE = " <<  FsObj::FAILED
					 << " WHERE FILE_NAME='" << fileName << "'"
					 << " AND REQ_NUM=" << reqNumber
					 << " AND TAPE_ID='" << tapeId << "'";
				TRACE(Trace::error, ssql.str());
				sqlite3_stmt *stmt2;
				sqlite3_statement::prepare(ssql.str(), &stmt2);
				rc2 = sqlite3_statement::step(stmt2);
				sqlite3_statement::checkRcAndFinalize(stmt2, rc2, SQLITE_DONE);
			}

			if ( time(NULL) - start < 10 )
				continue;

			start = time(NULL);

			std::lock_guard<std::mutex> lock(Scheduler::updmtx);
			Scheduler::updReq[reqNumber] = true;
			Scheduler::updcond.notify_all();
		}

		if ( rc == SQLITE_DONE )
			break;
	}

	sqlite3_statement::checkRcAndFinalize(stmt, rc, SQLITE_DONE);

	ssql.str("");
	ssql.clear();

	ssql << "UPDATE JOB_QUEUE SET FILE_STATE = " <<  toState
		 << " WHERE REQ_NUM=" << reqNumber
		 << " AND TAPE_ID='" << tapeId << "'"
		 << " AND (FILE_STATE=" << FsObj::RECALLING_MIG << " OR FILE_STATE=" << FsObj::RECALLING_PREMIG << ")"
		 << " AND I_NUM IN (";

	bool first = true;
	for ( unsigned long inum : inumList ) {
		if ( first ) {
			ssql << inum;
			first = false;
		}
		else {
			ssql << ", " << inum;
		}
	}
	ssql << ")";

	TRACE(Trace::normal, ssql.str());

	sqlite3_statement::prepare(ssql.str(), &stmt);
	rc = sqlite3_statement::step(stmt);
	sqlite3_statement::checkRcAndFinalize(stmt, rc, SQLITE_DONE);

	ssql.str("");
	ssql.clear();

	ssql << "UPDATE JOB_QUEUE SET FILE_STATE = " << FsObj::MIGRATED
		 << " WHERE REQ_NUM=" << reqNumber
		 << " AND TAPE_ID='" << tapeId << "'"
		 << " AND FILE_STATE=" << FsObj::RECALLING_MIG;

	TRACE(Trace::normal, ssql.str());

	sqlite3_statement::prepare(ssql.str(), &stmt);
	rc = sqlite3_statement::step(stmt);
	sqlite3_statement::checkRcAndFinalize(stmt, rc, SQLITE_DONE);

	ssql.str("");
	ssql.clear();

	ssql << "UPDATE JOB_QUEUE SET FILE_STATE = " << FsObj::PREMIGRATED
		 << " WHERE REQ_NUM=" << reqNumber
		 << " AND TAPE_ID='" << tapeId << "'"
		 << " AND FILE_STATE=" << FsObj::RECALLING_PREMIG;

	TRACE(Trace::normal, ssql.str());

	sqlite3_statement::prepare(ssql.str(), &stmt);
	rc = sqlite3_statement::step(stmt);
	sqlite3_statement::checkRcAndFinalize(stmt, rc, SQLITE_DONE);

	return suspended;
}

void SelRecall::execRequest(int reqNumber, int tgtState, std::string tapeId, bool needsTape)

{
	std::stringstream ssql;
	sqlite3_stmt *stmt;
	bool suspended = false;
	int rc;

	mrStatus.add(reqNumber);

	if ( tgtState == LTFSDmProtocol::LTFSDmMigRequest::PREMIGRATED )
		suspended = recallStep(reqNumber, tapeId, FsObj::PREMIGRATED, needsTape);
	else
		suspended = recallStep(reqNumber, tapeId, FsObj::RESIDENT, needsTape);

	std::unique_lock<std::mutex> lock(Scheduler::mtx);

	TRACE(Trace::always, reqNumber);
	TRACE(Trace::always, needsTape);

	if ( needsTape ) {
		std::lock_guard<std::recursive_mutex> lock(OpenLTFSInventory::mtx);
		inventory->getCartridge(tapeId)->setState(OpenLTFSCartridge::MOUNTED);
		bool found = false;
		for ( std::shared_ptr<OpenLTFSDrive> d : inventory->getDrives() ) {
			if ( d->get_slot() == inventory->getCartridge(tapeId)->get_slot() ) {
				TRACE(Trace::always, d->GetObjectID());
				d->setFree();
				d->clearToUnblock();
				found = true;
				break;
			}
		}
		assert(found == true);
	}

	std::unique_lock<std::mutex> updlock(Scheduler::updmtx);

	ssql.str("");
	ssql.clear();
	ssql << "UPDATE REQUEST_QUEUE SET STATE=" << (suspended ? DataBase::REQ_NEW : DataBase::REQ_COMPLETED)
		 << " WHERE REQ_NUM=" << reqNumber << " AND TAPE_ID='" << tapeId << "';";
	TRACE(Trace::normal, ssql.str());
	sqlite3_statement::prepare(ssql.str(), &stmt);
	rc = sqlite3_statement::step(stmt);
	sqlite3_statement::checkRcAndFinalize(stmt, rc, SQLITE_DONE);

	Scheduler::updReq[reqNumber] = true;
	Scheduler::updcond.notify_all();
	Scheduler::cond.notify_one();
}
