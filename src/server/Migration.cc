#include "ServerIncludes.h"

std::mutex Migration::pmigmtx;

FsObj::file_state Migration::checkState(std::string fileName, FsObj *fso)

{
	FsObj::file_state state;

	state = fso->getMigState();

	if ( fso->getMigState() == FsObj::RESIDENT ) {
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
			FsObj::mig_attr_t attr = fso->getAttribute();
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

	return state;
}


void Migration::addJob(std::string fileName)

{
	int replNum;
	struct stat statbuf;
	FsObj::file_state state;
	SQLStatement stmt;

	try {
		FsObj fso(fileName);
		stat(fileName.c_str(), &statbuf);

		if (!S_ISREG(statbuf.st_mode)) {
			MSG(LTFSDMS0018E, fileName);
			return;
		}

		state = checkState(fileName, &fso);

		stmt( Migration::ADD_MIGRATION_JOB )
			% DataBase::MIGRATION % fileName % reqNumber % targetState
			% statbuf.st_size % (long long) fso.getFsId() % fso.getIGen()
			% (long long) fso.getINode() % statbuf.st_mtim.tv_sec
			% statbuf.st_mtim.tv_nsec % time(NULL) % state;
	}
	catch ( const std::exception& e ) {
		TRACE(Trace::error, e.what());
		stmt( Migration::ADD_MIGRATION_JOB )
			% DataBase::MIGRATION % fileName % reqNumber % targetState
			% Const::UNSET % Const::UNSET % Const::UNSET % Const::UNSET
			% Const::UNSET % Const::UNSET % time(NULL) % FsObj::FAILED;
	}

	replNum = Const::UNSET;

	if ( pools.size() == 0 )
		pools.insert("");

	TRACE(Trace::normal, stmt.str());

	for ( std::string pool : pools ) {
		TRACE(Trace::full, stmt.str());
		try {
			replNum++;
			stmt.prepare();
			stmt.bind(1, replNum);
			stmt.bind(2, pool);
			stmt.step();
			stmt.finalize();
		}
		catch ( std::exception& e ) {
			TRACE(Trace::error, e.what());
			MSG(LTFSDMS0028E, fileName);
		}
		TRACE(Trace::always, fileName, replNum, pool);
	}

	jobnum++;

	return;
}

void Migration::addRequest()

{
	int replNum;
	std::stringstream ssql;
	SQLStatement stmt;
	std::stringstream thrdinfo;
	SubServer subs;

	for ( std::string pool : pools )
		TRACE(Trace::normal, pool);

	{
		std::lock_guard<std::mutex> updlock(Scheduler::updmtx);
		Scheduler::updReq[reqNumber] = false;
	}

	replNum = Const::UNSET;

	for (std::string pool : pools) {
		replNum++;
		std::unique_lock<std::mutex> lock(Scheduler::mtx);

		stmt( Migration::ADD_MIGRATION_REQUEST )
			% DataBase::MIGRATION % reqNumber % targetState % numReplica % replNum
			% pool % time(NULL) % ( needsTape ? DataBase::REQ_NEW : DataBase::REQ_INPROGRESS );

		TRACE(Trace::normal, stmt.str());

		stmt.doall();

		TRACE(Trace::always, needsTape, reqNumber, pool);

		if ( needsTape ) {
			Scheduler::cond.notify_one();
		}
		else {
			thrdinfo << "Mig(" << reqNumber << "," << replNum << "," << pool << ")";
			subs.enqueue(thrdinfo.str(), Migration::execRequest, reqNumber, targetState,
						 numReplica, replNum, pool, "", needsTape);
		}
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
	bool failed = false;

	try {
		FsObj source(mig_info.fileName);

		TRACE(Trace::always, mig_info.fileName);

		tapeName = Scheduler::getTapeName(mig_info.fileName, tapeId);

		fd = open(tapeName.c_str(), O_RDWR | O_CREAT);

		if ( fd == -1 ) {
			TRACE(Trace::error, errno);
			MSG(LTFSDMS0021E, tapeName.c_str());
			throw(EXCEPTION(Const::UNSET, tapeName, errno));
		}

		source.lock();

		source.preparePremigration();

		if ( stat(mig_info.fileName.c_str(), &statbuf) == -1 ) {
			TRACE(Trace::error, errno);
			MSG(LTFSDMS0040E, mig_info.fileName);
			throw(EXCEPTION(Const::UNSET, mig_info.fileName, errno));
		}
		if ( statbuf.st_mtim.tv_sec != secs || statbuf.st_mtim.tv_nsec != nsecs ) {
			TRACE(Trace::error, statbuf.st_mtim.tv_sec, secs, statbuf.st_mtim.tv_nsec, nsecs);
			MSG(LTFSDMS0041W, mig_info.fileName);
			throw(EXCEPTION(Const::UNSET, mig_info.fileName));
		}

		{
			std::lock_guard<std::mutex> writelock(writemtx);

			if ( inventory->getDrive(driveId)->getToUnblock() != DataBase::NOOP ) {
				TRACE(Trace::full, mig_info.fileName);
				source.remAttribute();
				std::lock_guard<std::mutex> lock(Migration::pmigmtx);
				*suspended = true;
				throw(EXCEPTION(Error::LTFSDM_OK));
			}

			while ( offset < statbuf.st_size ) {
				if ( Server::forcedTerminate )
					throw(EXCEPTION(Error::LTFSDM_OK));

				rsize = source.read(offset, statbuf.st_size - offset > Const::READ_BUFFER_SIZE ?
									Const::READ_BUFFER_SIZE : statbuf.st_size - offset , buffer);
				if ( rsize == -1 ) {
					TRACE(Trace::error, errno);
					MSG(LTFSDMS0023E, mig_info.fileName);
					throw(EXCEPTION(errno, errno, mig_info.fileName));
				}

				wsize = write(fd, buffer, rsize);

				if ( wsize != rsize ) {
					TRACE(Trace::error, errno, wsize, rsize);
					MSG(LTFSDMS0022E, tapeName.c_str());
					throw(EXCEPTION(Const::UNSET, mig_info.fileName, wsize, rsize));
				}

				offset += rsize;
				if ( stat(mig_info.fileName.c_str(), &statbuf_changed) == -1 ) {
					TRACE(Trace::error, errno);
					MSG(LTFSDMS0040E, mig_info.fileName);
					throw(EXCEPTION(Const::UNSET, mig_info.fileName, errno));
				}

				if ( statbuf_changed.st_mtim.tv_sec != secs || statbuf_changed.st_mtim.tv_nsec != nsecs ) {
					TRACE(Trace::error, statbuf_changed.st_mtim.tv_sec, secs, statbuf_changed.st_mtim.tv_nsec, nsecs);
					MSG(LTFSDMS0041W, mig_info.fileName);
					throw(EXCEPTION(Const::UNSET, mig_info.fileName));
				}
			}
		}

		if ( fsetxattr(fd, Const::LTFS_ATTR.c_str(), mig_info.fileName.c_str(), mig_info.fileName.length(), 0) == -1 ) {
			TRACE(Trace::error, errno);
			MSG(LTFSDMS0025E, Const::LTFS_ATTR, tapeName);
			throw(EXCEPTION(Const::UNSET, mig_info.fileName, errno));
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
	catch ( const OpenLTFSException& e ) {
		TRACE(Trace::error, e.what());
		if ( e.getError() != Error::LTFSDM_OK )
			failed = true;
	}
	catch ( const std::exception& e ) {
		TRACE(Trace::error, e.what());
		failed = true;
	}

	if ( failed == true ) {
		TRACE(Trace::error, mig_info.fileName);
		MSG(LTFSDMS0050E, mig_info.fileName);
		mrStatus.updateFailed(mig_info.reqNumber, mig_info.fromState);

		SQLStatement stmt = SQLStatement(Migration::FAIL_PREMIGRATION)
			% FsObj::FAILED % mig_info.reqNumber
			% mig_info.fileName % mig_info.replNum;

		stmt.doall();
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

		TRACE(Trace::always, mig_info. fileName);

		source.lock();
		attr = source.getAttribute();
		if ( mig_info.numRepl == 0 || attr.copies == mig_info.numRepl ) {
			source.prepareStubbing();
			source.stub();
			TRACE(Trace::full, mig_info. fileName);
		}
		source.unlock();

		std::lock_guard<std::mutex> lock(Migration::pmigmtx);
		inumList->push_back(mig_info.inum);
	}
	catch ( const std::exception& e ) {
		TRACE(Trace::error, e.what());
		MSG(LTFSDMS0089E, mig_info.fileName);

		for (int i=0; i<mig_info.numRepl; i++)
			mrStatus.updateFailed(mig_info.reqNumber, mig_info.fromState);

		SQLStatement stmt = SQLStatement(Migration::FAIL_STUBBING)
			% FsObj::FAILED % mig_info.reqNumber
			% mig_info.fileName;

		stmt.doall();
		return;
	}

	for (int i=0; i<mig_info.numRepl; i++)
		mrStatus.updateSuccess(mig_info.reqNumber, mig_info.fromState, mig_info.toState);
}

Migration::req_return_t Migration::migrationStep(int reqNumber, int numRepl, int replNum, std::string tapeId,
												 FsObj::file_state fromState, FsObj::file_state toState)

{
	SQLStatement stmt;
	std::string fileName;
	Migration::req_return_t retval = (Migration::req_return_t) {false, false};
	time_t start;
	long secs;
	long nsecs;
	unsigned long inum;
	time_t steptime;
	std::shared_ptr<std::list<unsigned long>> inumList =
		std::make_shared<std::list<unsigned long>>();
	std::shared_ptr<bool> suspended = std::make_shared<bool>(false);
	unsigned long freeSpace = 0;
	int num_found = 0;
	int total = 0;
	FsObj::file_state state;
	std::shared_ptr<OpenLTFSDrive> drive = nullptr;

	TRACE(Trace::always, reqNumber);

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
		stmt(Migration::SET_PREMIGRATING)
			% state % tapeId % reqNumber % fromState % replNum % (unsigned long) &freeSpace
			% (unsigned long) &num_found % (unsigned long) &total;
	}
	else {
		stmt(Migration::SET_STUBBING)
			% state % reqNumber % fromState % tapeId % replNum;
	}
	TRACE(Trace::normal, stmt.str());
	steptime = time(NULL);
	stmt.doall();

	TRACE(Trace::always, time(NULL) - steptime, num_found, total);
	if ( total > num_found )
		retval.remaining = true;

	stmt(Migration::SELECT_MIG_JOBS)
		% reqNumber % state % tapeId;
	TRACE(Trace::normal, stmt.str());
	stmt.prepare();
	start = time(NULL);
	while ( stmt.step(&fileName, &secs, &nsecs, &inum) ) {
		if ( Server::terminate == true )
			break;

		try {
			Migration::mig_info_t mig_info =
				{ fileName, reqNumber, numRepl, replNum, inum, "", fromState, toState };

			TRACE(Trace::always, fileName, reqNumber);

			if ( toState == FsObj::PREMIGRATED ) {
				if ( drive->getToUnblock() != DataBase::NOOP ) {
					retval.suspended = true;
					break;
				}
				TRACE(Trace::full, secs, nsecs);
				drive->wqp->enqueue(reqNumber, tapeId, drive->GetObjectID(), secs, nsecs, mig_info, inumList, suspended);
			}
			else {
				Scheduler::wqs->enqueue(reqNumber, mig_info, inumList);
			}
		}
		catch ( const std::exception& e ) {
			TRACE(Trace::error, e.what());
			continue;
		}

		if ( time(NULL) - start < 10 )
			continue;

		start = time(NULL);

		std::lock_guard<std::mutex> lock(Scheduler::updmtx);
		Scheduler::updReq[reqNumber] = true;
		Scheduler::updcond.notify_all();
	}
	std::lock_guard<std::mutex> lock(Scheduler::updmtx);
	Scheduler::updReq[reqNumber] = true;
	Scheduler::updcond.notify_all();
	stmt.finalize();

	if ( toState == FsObj::PREMIGRATED ) {
		drive->wqp->waitCompletion(reqNumber);
	}
	else {
		Scheduler::wqs->waitCompletion(reqNumber);
	}

	if ( *suspended == true )
		retval.suspended = true;

	stmt(Migration::SET_MIG_SUCCESS)
		% toState % reqNumber % state % tapeId % genInumString(*inumList);
	TRACE(Trace::normal, stmt.str());
	steptime = time(NULL);
	stmt.doall();
	TRACE(Trace::always, time(NULL) - steptime);

	stmt(Migration::SET_MIG_RESET)
		% fromState % reqNumber % state;
	TRACE(Trace::normal, stmt.str());
	steptime = time(NULL);
	stmt.doall();
	TRACE(Trace::always, time(NULL) - steptime);

	return retval;
}

void Migration::execRequest(int reqNumber, int targetState, int numRepl,
							int replNum, std::string pool, std::string tapeId, bool needsTape)

{
	TRACE(Trace::full, __PRETTY_FUNCTION__);

	SQLStatement stmt;
	std::stringstream tapePath;
	Migration::req_return_t retval = (Migration::req_return_t) {false, false};
	bool failed = false;

	mrStatus.add(reqNumber);

	TRACE(Trace::always, reqNumber, needsTape);

	if ( needsTape ) {
		retval = migrationStep(reqNumber, numRepl, replNum, tapeId,  FsObj::RESIDENT, FsObj::PREMIGRATED);

		tapePath << inventory->getMountPoint() << "/" << tapeId;

		if ( setxattr(tapePath.str().c_str(), Const::LTFS_SYNC_ATTR.c_str(),
					  Const::LTFS_SYNC_VAL.c_str(), Const::LTFS_SYNC_VAL.length(), 0) == -1 ) {
			if ( errno != ENODATA ) {
				TRACE(Trace::error, errno);
				MSG(LTFSDMS0024E, tapeId);

				stmt(Migration::FAIL_PREMIGRATED)
					% FsObj::FAILED % reqNumber % FsObj::PREMIGRATED % tapeId % replNum;

				TRACE(Trace::error, stmt.str());

				stmt.doall();

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
				TRACE(Trace::always, d->GetObjectID());
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

	if ( retval.suspended )
		stmt(Migration::UPDATE_MIG_REQUEST)
			% DataBase::REQ_NEW % reqNumber % replNum;
	else if ( retval.remaining )
		stmt(Migration::UPDATE_MIG_REQUEST_RESET_TAPE)
			% DataBase::REQ_NEW % reqNumber % replNum;
	else
		stmt(Migration::UPDATE_MIG_REQUEST)
			% DataBase::REQ_COMPLETED % reqNumber % replNum;

	TRACE(Trace::normal, stmt.str());

	stmt.doall();

	Scheduler::updReq[reqNumber] = true;
	Scheduler::updcond.notify_all();
}
