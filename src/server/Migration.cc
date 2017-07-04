#include "ServerIncludes.h"

std::mutex Migration::pmigmtx;

void Migration::addJob(std::string fileName)

{
	int rc;
	int replNum;
	struct stat statbuf;
	std::stringstream ssql;
	sqlite3_stmt *stmt;

	ssql.str("");
	ssql.clear();
	ssql << "INSERT INTO JOB_QUEUE (OPERATION, FILE_NAME, REQ_NUM, TARGET_STATE, REPL_NUM, TAPE_POOL, "
		 << "FILE_SIZE, FS_ID, I_GEN, I_NUM, MTIME_SEC, MTIME_NSEC, LAST_UPD, TAPE_ID, FILE_STATE) "
		 << "VALUES (" << DataBase::MIGRATION << ", "            // OPERATION
		 << "'" << fileName << "', "                             // FILE_NAME
		 << reqNumber << ", "                                    // REQ_NUM
		 << targetState << ", "                                  // MIGRATION_STATE
		 << "?, "                                                // REPL_NUM
		 << "?, ";                                               // TAPE_POOL

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
			 << "'', ";                                          // TAPE_ID

		FsObj::file_state state = fso.getMigState();

		if ( fso.getMigState() == FsObj::RESIDENT ) {
			if ( numReplica == 0 ) {
				MSG(LTFSDMS0065E, fileName);
				state = FsObj::FAILED;
			}
			else {
				needsTape = true;
			}
		}
		else {
			if ( numReplica != 0 ) {
				FsObj::mig_attr_t attr = fso.getAttribute();
				for ( int i=0; i<3; i++ ) {
					if ( std::string("").compare(attr.tapeId[i]) == 0 ) {
						if ( i == 0 ) {
							MSG(LTFSDMS0066E, fileName);
							state = FsObj::FAILED;
							break;
						}
						else {
							break;
						}
					}
					bool tapeFound = false;
					for ( std::string pool : pools ) {
						std::lock_guard<std::recursive_mutex> lock(OpenLTFSInventory::mtx);
						std::list<std::shared_ptr<OpenLTFSCartridge>> carts
							= inventory->getPool(pool)->getCartridges();
						for ( std::shared_ptr<OpenLTFSCartridge> cart : carts ) {
							if ( cart->GetObjectID().compare(attr.tapeId[i]) == 0 ) {
								tapeFound = true;
								break;
							}
						}
						if ( tapeFound )
							break;
					}
					if ( !tapeFound ) {
						MSG(LTFSDMS0067E, fileName, attr.tapeId[i]);
						state = FsObj::FAILED;
						break;
					}
				}
			}
		}
		ssql << state << ");";   // FILE_STATE
	}
	catch ( int error ) {
		ssql.str("");
		ssql.clear();
		ssql << "INSERT INTO JOB_QUEUE (OPERATION, FILE_NAME, REQ_NUM, TARGET_STATE, REPL_NUM, TAPE_POOL, "
			 << "FILE_SIZE, FS_ID, I_GEN, I_NUM, MTIME_SEC, MTIME_NSEC, LAST_UPD, FILE_STATE) "
			 << "VALUES (" << DataBase::MIGRATION << ", "        // OPERATION
			 << "'" << fileName << "', "                         // FILE_NAME
			 << reqNumber << ", "                                // REQ_NUM
			 << targetState << ", "                              // MIGRATION_STATE
			 << "?, "                                            // REPL_NUM
			 << "?, "                                            // TAPE_POOL
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

	replNum = 0;

	if ( pools.size() == 0 )
		pools.insert("");

	for ( std::string pool : pools ) {
		TRACE(Trace::much, ssql.str());
		sqlite3_statement::prepare(ssql.str(), &stmt);
		if ( (rc = sqlite3_bind_int(stmt, 1, replNum)) != SQLITE_OK ) {
			TRACE(Trace::error, rc);
			MSG(LTFSDMS0028E, fileName.c_str());
			return;
		}
		if ( (rc = sqlite3_bind_text(stmt, 2, pool.c_str(), pool.size(), 0)) != SQLITE_OK ) {
			TRACE(Trace::error, rc);
			MSG(LTFSDMS0028E, fileName.c_str());
			return;
		}
		replNum++;
		rc = sqlite3_step(stmt);
		sqlite3_statement::checkRcAndFinalize(stmt, rc, SQLITE_DONE);
	}

	jobnum++;

	return;
}

void Migration::addRequest()

{
	int rc;
	int replNum;
	std::stringstream ssql;
	sqlite3_stmt *stmt;
	std::string tapeId;
	std::stringstream thrdinfo;
	SubServer subs;

	for ( std::string pool : pools )
		TRACE(Trace::little, pool);

	{
		std::lock_guard<std::mutex> updlock(Scheduler::updmtx);
		Scheduler::updReq[reqNumber] = false;
	}

	replNum = 0;
	for (std::string pool : pools) {
		std::unique_lock<std::mutex> lock(Scheduler::mtx);

		ssql.str("");
		ssql.clear();

		tapeId = "";

		ssql  << "INSERT INTO REQUEST_QUEUE (OPERATION, REQ_NUM, TARGET_STATE, "
			  << "NUM_REPL, REPL_NUM, TAPE_POOL, TAPE_ID, TIME_ADDED, STATE) "
			  << "VALUES (" << DataBase::MIGRATION << ", "                         // OPERATION
			  << reqNumber << ", "                                                 // FILE_NAME
			  << targetState << ", "                                               // TARGET_STATE
			  << numReplica << ", "                                                // NUM_REPL
			  << replNum << ", "                                                   // REPL_NUM
			  << "'" << pool << "', "                                              // TAPE_POOL
			  << "'" << tapeId << "', "                                            // TAPE_ID
			  << time(NULL) << ", "                                                // TIME_ADDED
			  << ( needsTape ? DataBase::REQ_NEW : DataBase::REQ_INPROGRESS )      // STATE
			  << ");";

		sqlite3_statement::prepare(ssql.str(), &stmt);
		TRACE(Trace::always, std::string(ssql.str()));

		rc = sqlite3_statement::step(stmt);

		sqlite3_statement::checkRcAndFinalize(stmt, rc, SQLITE_DONE);

		if ( needsTape ) {
			Scheduler::cond.notify_one();
		}
		else {
			thrdinfo << "Mig(" << reqNumber << "," << replNum << "," << pool << ")";
			subs.enqueue(thrdinfo.str(), Migration::execRequest, reqNumber, targetState,
						 numReplica, replNum, pool, tapeId, needsTape);
		}
		replNum++;
	}

	subs.waitAllRemaining();
}

std::mutex writemtx;


unsigned long Migration::preMigrate(std::string tapeId, std::string driveId, long secs, long nsecs,
									Migration::mig_info_t mig_info,
									std::shared_ptr<std::list<unsigned long>> inumList, std::shared_ptr<bool> suspended)

{
	struct stat statbuf, statbuf_changed;
	std::string tapeName;
	char buffer[Const::READ_BUFFER_SIZE];
	long rsize;
	long wsize;
	int fd = -1;
	long offset = 0;
	FsObj::mig_attr_t attr;

	try {
		FsObj source(mig_info.fileName);

		TRACE(Trace::much, mig_info.fileName);

		tapeName = Scheduler::getTapeName(mig_info.fileName, tapeId);

		fd = open(tapeName.c_str(), O_RDWR | O_CREAT);

		if ( fd == -1 ) {
			TRACE(Trace::error, errno);
			MSG(LTFSDMS0021E, tapeName.c_str());
			throw(Error::LTFSDM_GENERAL_ERROR);
		}

		source.lock();

		source.preparePremigration();

		if ( stat(mig_info.fileName.c_str(), &statbuf) == -1 ) {
			TRACE(Trace::error, errno);
			MSG(LTFSDMS0040E, mig_info.fileName);
			throw(Error::LTFSDM_GENERAL_ERROR);
		}
		if ( statbuf.st_mtim.tv_sec != secs || statbuf.st_mtim.tv_nsec != nsecs ) {
			TRACE(Trace::error, statbuf.st_mtim.tv_sec);
			TRACE(Trace::error, secs);
			TRACE(Trace::error, statbuf.st_mtim.tv_nsec);
			TRACE(Trace::error, nsecs);
			MSG(LTFSDMS0041W, mig_info.fileName);
			throw(Error::LTFSDM_GENERAL_ERROR);
		}

		{
			std::lock_guard<std::mutex> writelock(writemtx);

			if ( inventory->getDrive(driveId)->getToUnblock() != DataBase::NOOP ) {
				TRACE(Trace::much, mig_info.fileName);
				source.remAttribute();
				std::lock_guard<std::mutex> lock(Migration::pmigmtx);
				*suspended = true;
				throw(Error::LTFSDM_OK);
			}

			while ( offset < statbuf.st_size ) {
				if ( Server::forcedTerminate )
					throw(Error::LTFSDM_OK);

				rsize = source.read(offset, statbuf.st_size - offset > Const::READ_BUFFER_SIZE ?
									Const::READ_BUFFER_SIZE : statbuf.st_size - offset , buffer);
				if ( rsize == -1 ) {
					TRACE(Trace::error, errno);
					MSG(LTFSDMS0023E, mig_info.fileName);
					throw(errno);
				}

				wsize = write(fd, buffer, rsize);

				if ( wsize != rsize ) {
					TRACE(Trace::error, errno);
					TRACE(Trace::error, wsize);
					TRACE(Trace::error, rsize);
					MSG(LTFSDMS0022E, tapeName.c_str());
					throw(Error::LTFSDM_GENERAL_ERROR);
				}

				offset += rsize;
				if ( stat(mig_info.fileName.c_str(), &statbuf_changed) == -1 ) {
					TRACE(Trace::error, errno);
					MSG(LTFSDMS0040E, mig_info.fileName);
					throw(Error::LTFSDM_GENERAL_ERROR);
				}

				if ( statbuf_changed.st_mtim.tv_sec != secs || statbuf_changed.st_mtim.tv_nsec != nsecs ) {
					TRACE(Trace::error, statbuf_changed.st_mtim.tv_sec);
					TRACE(Trace::error, secs);
					TRACE(Trace::error, statbuf_changed.st_mtim.tv_nsec);
					TRACE(Trace::error, nsecs);
					MSG(LTFSDMS0041W, mig_info.fileName);
					throw(Error::LTFSDM_GENERAL_ERROR);
				}
			}
		}

		if ( fsetxattr(fd, Const::LTFS_ATTR.c_str(), mig_info.fileName.c_str(), mig_info.fileName.length(), 0) == -1 ) {
			TRACE(Trace::error, errno);
			MSG(LTFSDMS0025E, Const::LTFS_ATTR, tapeName);
			throw(errno);
		}

		mrStatus.updateSuccess(mig_info.reqNumber, mig_info.fromState, mig_info.toState);

		attr = source.getAttribute();
		memset(attr.tapeId[attr.copies], 0, Const::tapeIdLength+1);
		strncpy(attr.tapeId[attr.copies], tapeId.c_str(), Const::tapeIdLength);
		attr.copies++;
		source.addAttribute(attr);
		if ( attr.copies == mig_info.numRepl )
			source.finishPremigration();
		source.unlock();

		std::lock_guard<std::mutex> lock(Migration::pmigmtx);
		inumList->push_back(mig_info.inum);
	}
	catch ( int error ) {
		sqlite3_stmt *stmt;
		std::stringstream ssql;
		int rc;

		if ( error != Error::LTFSDM_OK ) {
			TRACE(Trace::error, mig_info.fileName);
			MSG(LTFSDMS0050E, mig_info.fileName);

			mrStatus.updateFailed(mig_info.reqNumber, mig_info.fromState);
			ssql << "UPDATE JOB_QUEUE SET FILE_STATE=" <<  FsObj::FAILED
				 << " WHERE REQ_NUM=" << mig_info.reqNumber
				 << " AND FILE_NAME='" << mig_info.fileName << "'"
				 << " AND REPL_NUM=" << mig_info.replNum;
			sqlite3_statement::prepare(ssql.str(), &stmt);
			rc = sqlite3_statement::step(stmt);
			sqlite3_statement::checkRcAndFinalize(stmt, rc, SQLITE_DONE);
		}
	}

	if ( fd != -1 )
		close(fd);

	return statbuf.st_size;
}

void Migration::stub(Migration::mig_info_t mig_info, std::shared_ptr<std::list<unsigned long>> inumList)

{
	try {
		FsObj source(mig_info.fileName);
		FsObj::mig_attr_t attr;

		TRACE(Trace::much, mig_info. fileName);

		source.lock();
		attr = source.getAttribute();
		if ( mig_info.numRepl == 0 || attr.copies == mig_info.numRepl ) {
			source.prepareStubbing();
			source.stub();
			TRACE(Trace::much, mig_info. fileName);
		}
		source.unlock();

		std::lock_guard<std::mutex> lock(Migration::pmigmtx);
		inumList->push_back(mig_info.inum);
	}
	catch ( int error ) {
		sqlite3_stmt *stmt;
		std::stringstream ssql;
		int rc;

		mrStatus.updateFailed(mig_info.reqNumber, mig_info.fromState);
		ssql << "UPDATE JOB_QUEUE SET FILE_STATE=" <<  FsObj::FAILED
			 << " WHERE REQ_NUM=" << mig_info.reqNumber
			 << " AND REPL_NUM=" << mig_info.replNum;
		TRACE(Trace::error, ssql.str());
		sqlite3_statement::prepare(ssql.str(), &stmt);
		rc = sqlite3_statement::step(stmt);
		sqlite3_statement::checkRcAndFinalize(stmt, rc, SQLITE_DONE);
	}

	mrStatus.updateSuccess(mig_info.reqNumber, mig_info.fromState, mig_info.toState);
}

Migration::req_return_t Migration::migrationStep(int reqNumber, int numRepl, int replNum, std::string tapeId,
												 FsObj::file_state fromState, FsObj::file_state toState)

{
	sqlite3_stmt *stmt;
	std::stringstream ssql;
	int rc, rc2;
	Migration::req_return_t retval = (Migration::req_return_t) {false, false};
	time_t start;
	long secs;
	long nsecs;
	time_t steptime;
	std::shared_ptr<std::list<unsigned long>> inumList =
		std::make_shared<std::list<unsigned long>>();
	std::shared_ptr<bool> suspended = std::make_shared<bool>(false);
	unsigned long freeSpace = 0;
	int num_found = 0;
	int total = 0;
	FsObj::file_state state;
	std::shared_ptr<OpenLTFSDrive> drive = nullptr;

	TRACE(Trace::much, reqNumber);

	{
		std::lock_guard<std::mutex> lock(Scheduler::updmtx);
		Scheduler::updReq[reqNumber] = true;
		Scheduler::updcond.notify_all();
	}

	if ( toState == FsObj::PREMIGRATED ) {
		for ( std::shared_ptr<OpenLTFSDrive> d : inventory->getDrives() ) {
			if ( d->get_slot() == inventory->getCartridge(tapeId)->get_slot() ) {
				drive = d;
				break;
			}
		}
		assert(drive != nullptr);
	}

	state = ((toState == FsObj::PREMIGRATED) ? FsObj::PREMIGRATING : FsObj::STUBBING);

	if ( toState == FsObj::PREMIGRATED ) {
		freeSpace = 1024*1024*inventory->getCartridge(tapeId)->get_remaining_cap();
		ssql << "UPDATE JOB_QUEUE SET FILE_STATE=" <<  state
			 << ", TAPE_ID='" << tapeId << "'"
			 << " WHERE REQ_NUM=" << reqNumber
			 << " AND FILE_STATE=" << fromState
			 << " AND REPL_NUM=" << replNum
			 << " AND FITS(I_NUM, FILE_SIZE, "
			 << (unsigned long) &freeSpace
			 << "," << (unsigned long) &num_found
			 << "," << (unsigned long) &total << ")=1";
	}
	else {
		ssql << "UPDATE JOB_QUEUE SET FILE_STATE=" <<  state
			 << " WHERE REQ_NUM=" << reqNumber
			 << " AND FILE_STATE=" << fromState
			 << " AND TAPE_ID='" << tapeId << "'"
			 << " AND REPL_NUM=" << replNum;
	}

	steptime = time(NULL);

	sqlite3_statement::prepare(ssql.str(), &stmt);
	rc2 = sqlite3_statement::step(stmt);
	sqlite3_statement::checkRcAndFinalize(stmt, rc2, SQLITE_DONE);

	TRACE(Trace::always, time(NULL) - steptime);

	TRACE(Trace::always, num_found);
	TRACE(Trace::always, total);

	if ( total > num_found )
		retval.remaining = true;

	ssql.str("");
	ssql.clear();

	ssql << "SELECT FILE_NAME, MTIME_SEC, MTIME_NSEC, I_NUM FROM JOB_QUEUE WHERE"
		 << " REQ_NUM=" << reqNumber
		 << " AND FILE_STATE=" << state
		 << " AND TAPE_ID='" << tapeId << "'";

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
			const char *cstr = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
			secs = sqlite3_column_int64(stmt, 1);
			nsecs = sqlite3_column_int64(stmt, 2);
			unsigned long inum = static_cast<unsigned long>(sqlite3_column_int64(stmt, 3));

			if (!cstr)
				continue;
			else
				fileName = std::string(cstr);

			try {
				Migration::mig_info_t mig_info;
				mig_info.fileName = fileName;
				mig_info.inum = inum;
				mig_info.reqNumber = reqNumber;
				mig_info.numRepl = numRepl;
				mig_info.replNum = replNum;
				mig_info.fromState = fromState;
				mig_info.toState = toState;

				if ( toState == FsObj::PREMIGRATED ) {
					if ( drive->getToUnblock() != DataBase::NOOP ) {
						retval.suspended = true;
						rc = SQLITE_DONE;
						break;
					}
					TRACE(Trace::much, secs);
					TRACE(Trace::much, nsecs);
					drive->wqp->enqueue(reqNumber, tapeId, drive->GetObjectID(), secs, nsecs, mig_info, inumList, suspended);
				}
				else {
					Scheduler::wqs->enqueue(reqNumber, mig_info, inumList);
				}
			}
			catch (int error) {
				TRACE(Trace::error, error);
				continue;
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

	if ( toState == FsObj::PREMIGRATED ) {
		drive->wqp->waitCompletion(reqNumber);
	}
	else {
		Scheduler::wqs->waitCompletion(reqNumber);
	}

	if ( *suspended == true )
		retval.suspended = true;

	ssql.str("");
	ssql.clear();

	ssql << "UPDATE JOB_QUEUE SET FILE_STATE=" <<  toState
		 << " WHERE REQ_NUM=" << reqNumber
		 << " AND FILE_STATE=" << state
		 << " AND TAPE_ID='" << tapeId << "'"
		 << " AND I_NUM IN (";

	bool first = true;
	for ( unsigned long inum : *inumList ) {
		if ( first ) {
			ssql << inum;
			first = false;
		}
		else {
			ssql << ", " << inum;
		}
	}
	ssql << ")";

	steptime = time(NULL);

	sqlite3_statement::prepare(ssql.str(), &stmt);
	rc2 = sqlite3_statement::step(stmt);
	sqlite3_statement::checkRcAndFinalize(stmt, rc2, SQLITE_DONE);

	TRACE(Trace::always, time(NULL) - steptime);

	ssql.str("");
	ssql.clear();

	ssql << "UPDATE JOB_QUEUE SET FILE_STATE=" <<  fromState
		 << " WHERE REQ_NUM=" << reqNumber
		 << " AND FILE_STATE=" << state;

	steptime = time(NULL);

	sqlite3_statement::prepare(ssql.str(), &stmt);
	rc2 = sqlite3_statement::step(stmt);
	sqlite3_statement::checkRcAndFinalize(stmt, rc2, SQLITE_DONE);

	TRACE(Trace::always, time(NULL) - steptime);

	return retval;
}

void Migration::execRequest(int reqNumber, int targetState, int numRepl,
							int replNum, std::string pool, std::string tapeId, bool needsTape)

{
	TRACE(Trace::much, __PRETTY_FUNCTION__);

	sqlite3_stmt *stmt;
	std::stringstream ssql;
	int rc;
	std::stringstream tapePath;
	Migration::req_return_t retval = (Migration::req_return_t) {false, false};
	bool failed = false;

	mrStatus.add(reqNumber);

	if ( needsTape ) {
		retval = migrationStep(reqNumber, numRepl, replNum, tapeId,  FsObj::RESIDENT, FsObj::PREMIGRATED);

		tapePath << Const::LTFS_PATH << "/" << tapeId;

		if ( setxattr(tapePath.str().c_str(), Const::LTFS_SYNC_ATTR.c_str(),
					  Const::LTFS_SYNC_VAL.c_str(), Const::LTFS_SYNC_VAL.length(), 0) == -1 ) {
			if ( errno != ENODATA ) {
				TRACE(Trace::error, errno);
				MSG(LTFSDMS0024E, tapeId);

				ssql.str("");
				ssql.clear();

				ssql << "UPDATE JOB_QUEUE SET FILE_STATE = " <<  FsObj::FAILED
					 << " WHERE REQ_NUM=" << reqNumber
					 << " AND FILE_STATE=" << FsObj::PREMIGRATED
					 << " AND TAPE_ID='" << tapeId << "'"
					 << " AND REPL_NUM=" << replNum;

				TRACE(Trace::error, ssql.str());

				sqlite3_statement::prepare(ssql.str(), &stmt);
				rc = sqlite3_statement::step(stmt);
				sqlite3_statement::checkRcAndFinalize(stmt, rc, SQLITE_DONE);

				std::unique_lock<std::mutex> lock(Scheduler::updmtx);
				TRACE(Trace::error, reqNumber);
				Scheduler::updReq[reqNumber] = true;
				Scheduler::updcond.notify_all();

				failed = true;
			}
		}

		inventory->update(inventory->getCartridge(tapeId));

		std::lock_guard<std::recursive_mutex> lock(OpenLTFSInventory::mtx);
		inventory->getCartridge(tapeId)->setState(OpenLTFSCartridge::MOUNTED);
		bool found = false;
		for ( std::shared_ptr<OpenLTFSDrive> d : inventory->getDrives() ) {
			if ( d->get_slot() == inventory->getCartridge(tapeId)->get_slot() ) {
				TRACE(Trace::always, std::string("SET FREE: ") + d->GetObjectID());
				d->setFree();
				d->clearToUnblock();
				found = true;
				break;
			}
		}
		assert(found == true);
		Scheduler::cond.notify_one();
	}

	if ( !failed && targetState != LTFSDmProtocol::LTFSDmMigRequest::PREMIGRATED )
		migrationStep(reqNumber, numRepl, replNum, tapeId,  FsObj::PREMIGRATED, FsObj::MIGRATED);

	std::unique_lock<std::mutex> updlock(Scheduler::updmtx);

	ssql.str("");
	ssql.clear();
	if ( retval.suspended )
		ssql << "UPDATE REQUEST_QUEUE SET STATE=" << DataBase::REQ_NEW;
	else if ( retval.remaining )
		ssql << "UPDATE REQUEST_QUEUE SET STATE=" << DataBase::REQ_NEW << ", TAPE_ID=''" ;
	else
		ssql << "UPDATE REQUEST_QUEUE SET STATE=" << DataBase::REQ_COMPLETED;

	ssql << " WHERE REQ_NUM=" << reqNumber
		 << " AND REPL_NUM=" << replNum << ";";

	sqlite3_statement::prepare(ssql.str(), &stmt);
	rc = sqlite3_statement::step(stmt);
	sqlite3_statement::checkRcAndFinalize(stmt, rc, SQLITE_DONE);
	Scheduler::updReq[reqNumber] = true;

	Scheduler::updcond.notify_all();
}
