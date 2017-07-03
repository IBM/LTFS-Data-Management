#include "ServerIncludes.h"

std::mutex Scheduler::mtx;
std::condition_variable Scheduler::cond;
std::mutex Scheduler::updmtx;
std::condition_variable Scheduler::updcond;
std::map<int, std::atomic<bool>> Scheduler::updReq;

ThreadPool<Migration::mig_info_t, std::shared_ptr<std::list<unsigned long>>> *Scheduler::wqs;

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


std::string Scheduler::getTapeName(unsigned long long fsid, unsigned int igen,
								   unsigned long long ino, std::string tapeId)

{
	std::stringstream tapeName;

	tapeName << Const::LTFS_PATH
			 << "/" << tapeId << "/"
			 << Const::LTFS_NAME << "."
			 << fsid << "."
			 << igen << "."
			 << ino;

	return tapeName.str();
}

bool Scheduler::poolResAvail(unsigned long minFileSize)

{
	bool found;
	bool unmountedExists = false;

	assert(pool.compare("") != 0 );

	for ( std::shared_ptr<OpenLTFSCartridge> card : inventory->getPool(pool)->getCartridges() ) {
		if ( card->getState() == OpenLTFSCartridge::MOUNTED ) {
			tapeId = card->GetObjectID();
			found = false;
			for ( std::shared_ptr<OpenLTFSDrive> drive : inventory->getDrives() ) {
				if ( drive->get_slot() == card->get_slot() &&
					 1024*1024*card->get_remaining_cap() >= minFileSize ) {
					assert(drive->isBusy() == false);
					card->setState(OpenLTFSCartridge::INUSE);
					TRACE(Trace::always, std::string("SET BUSY: ") + drive->GetObjectID());
					drive->setBusy();
					found = true;
					break;
				}
			}
			assert(found == true || 1024*1024*card->get_remaining_cap() < minFileSize);
			if ( found == true )
				return true;
		}
		else if ( card->getState() == OpenLTFSCartridge::UNMOUNTED )
			unmountedExists = true;
	}

	if ( unmountedExists == false )
		return false;

	for ( std::shared_ptr<OpenLTFSDrive> drive : inventory->getDrives() ) {
		if ( drive->isBusy() == true )
			continue;
		found = false;
		for ( std::shared_ptr<OpenLTFSCartridge> card : inventory->getCartridges() ) {
			if ( drive->get_slot() == card->get_slot() &&
				 card->getState() == OpenLTFSCartridge::MOUNTED ) {
				found = true;
				break;
			}
		}
		if ( found == false ) {
			for ( std::shared_ptr<OpenLTFSCartridge> card : inventory->getPool(pool)->getCartridges() ) {
				if ( card->getState() == OpenLTFSCartridge::UNMOUNTED &&
					 1024*1024*card->get_remaining_cap() >= minFileSize ) {
					TRACE(Trace::always, std::string("SET BUSY: ") + drive->GetObjectID());
					drive->setBusy();
					drive->setUnmountReqNum(reqNum);
					card->setState(OpenLTFSCartridge::MOVING);
					subs.enqueue(std::string("m:") + card->GetObjectID(), mount,
											 drive->GetObjectID(), card->GetObjectID());
					return false;
				}
			}

		}
	}

	for ( std::shared_ptr<OpenLTFSDrive> drive : inventory->getDrives() )
		if ( drive->getUnmountReqNum() == reqNum )
			return false;

	for ( std::shared_ptr<OpenLTFSDrive> drive : inventory->getDrives() ) {
		if ( drive->isBusy() == true )
			continue;
		for ( std::shared_ptr<OpenLTFSCartridge> card : inventory->getCartridges() ) {
			if ( (drive->get_slot() == card->get_slot()) &&
				 (card->getState() == OpenLTFSCartridge::MOUNTED)) {
				TRACE(Trace::always, std::string("SET BUSY: ") + drive->GetObjectID());
				drive->setBusy();
				drive->setUnmountReqNum(reqNum);
				card->setState(OpenLTFSCartridge::MOVING);
				subs.enqueue(std::string("unm:") + card->GetObjectID(), unmount,
										 drive->GetObjectID(), card->GetObjectID());
				return false;
			}
		}
	}

	return false;
}


bool Scheduler::tapeResAvail()

{
	bool found;

	assert(tapeId.compare("") != 0 );

	if ( inventory->getCartridge(tapeId)->getState() == OpenLTFSCartridge::MOVING )
		return false;

	if ( inventory->getCartridge(tapeId)->getState() == OpenLTFSCartridge::INUSE ) {
		found = false;
		for ( std::shared_ptr<OpenLTFSDrive> drive : inventory->getDrives() ) {
			if ( drive->get_slot() == inventory->getCartridge(tapeId)->get_slot() ) {
				drive->setToUnblock(op);
				found = true;
				break;
			}
		}
		assert(found == true );
		return false;
	}
	else if ( inventory->getCartridge(tapeId)->getState() == OpenLTFSCartridge::MOUNTED ) {
		found = false;
		for ( std::shared_ptr<OpenLTFSDrive> drive : inventory->getDrives() ) {
			if ( drive->get_slot() == inventory->getCartridge(tapeId)->get_slot() ) {
				inventory->getCartridge(tapeId)->setState(OpenLTFSCartridge::INUSE);
				assert(drive->isBusy() == false);
				TRACE(Trace::always, std::string("SET BUSY: ") + drive->GetObjectID());
				drive->setBusy();
				found = true;
				break;
			}
		}
		assert(found == true );
		return true;
	}

	// looking for a free drive
	for ( std::shared_ptr<OpenLTFSDrive> drive : inventory->getDrives() ) {
		if ( drive->isBusy() == true )
			continue;
		found = false;
		for ( std::shared_ptr<OpenLTFSCartridge> card : inventory->getCartridges() ) {
			if ( (drive->get_slot() == card->get_slot()) &&
				 (card->getState() == OpenLTFSCartridge::MOUNTED)) {
				found = true;
				break;
			}
		}
		if ( found == false ) {
			if ( inventory->getCartridge(tapeId)->getState() == OpenLTFSCartridge::UNMOUNTED ) {
				TRACE(Trace::always, std::string("SET BUSY: ") + drive->GetObjectID());
				drive->setBusy();
				drive->setUnmountReqNum(reqNum);
				inventory->getCartridge(tapeId)->setState(OpenLTFSCartridge::MOVING);
				subs.enqueue(std::string("m:") + tapeId, mount, drive->GetObjectID(), tapeId);
				return false;
			}
		}
	}

	// looking for a tape to unmount
	for ( std::shared_ptr<OpenLTFSDrive> drive : inventory->getDrives() ) {
		if ( drive->isBusy() == true )
			continue;
		for ( std::shared_ptr<OpenLTFSCartridge> card : inventory->getCartridges() ) {
			if ( (drive->get_slot() == card->get_slot()) &&
				 (card->getState() == OpenLTFSCartridge::MOUNTED)) {
				TRACE(Trace::always, std::string("SET BUSY: ") + drive->GetObjectID());
				drive->setBusy();
				drive->setUnmountReqNum(reqNum);
				card->setState(OpenLTFSCartridge::MOVING);
				inventory->getCartridge(tapeId)->unsetRequested();
				subs.enqueue(std::string("unm:") + card->GetObjectID(), unmount,
										 drive->GetObjectID(), card->GetObjectID());
				return false;
			}
		}
	}

	if ( inventory->getCartridge(tapeId)->isRequested() )
		return false;

	// suspend an operation
	for ( std::shared_ptr<OpenLTFSDrive> drive : inventory->getDrives() ) {
		if ( op < drive->getToUnblock() ) {
			drive->setToUnblock(op);
			inventory->getCartridge(tapeId)->setRequested();
			break;
		}
	}

	return false;
}


bool Scheduler::resAvail(unsigned long minFileSize)

{
	if ( op == DataBase::MIGRATION && tapeId.compare("") == 0 )
		return poolResAvail(minFileSize);
	else
		return tapeResAvail();
}

long Scheduler::getStartBlock(std::string tapeName)

{
	long size;
	char startBlockStr[32];
	long startBlock;

	memset(startBlockStr, 0, sizeof(startBlockStr));

	size = getxattr(tapeName.c_str(), Const::LTFS_START_BLOCK.c_str(), startBlockStr, sizeof(startBlockStr));

	if ( size == -1 ) {
		TRACE(Trace::error, tapeName.c_str());
		TRACE(Trace::error, errno);
		return Const::UNSET;
	}

	startBlock = strtol(startBlockStr, NULL, 0);

	if ( startBlock == LONG_MIN || startBlock == LONG_MAX )
		return Const::UNSET;
	else
		return startBlock;
}

unsigned long Scheduler::smallestMigJob(int reqNum, int replNum)

{
	sqlite3_stmt *stmt;
	std::stringstream ssql;
	int rc;
	unsigned long min;

	ssql << "SELECT MIN(FILE_SIZE) FROM JOB_QUEUE WHERE"
		 << " REQ_NUM=" << reqNum
		 << " AND FILE_STATE=" << FsObj::RESIDENT
		 << " AND REPL_NUM=" << replNum;

	sqlite3_statement::prepare(ssql.str(), &stmt);

	rc = sqlite3_statement::step(stmt);

	min = (unsigned long) sqlite3_column_int64(stmt, 0);

	sqlite3_statement::checkRcAndFinalize(stmt, rc, SQLITE_ROW);

	return min;
}

void Scheduler::run(long key)

{
	TRACE(Trace::much, __PRETTY_FUNCTION__);

	sqlite3_stmt *stmt;
	std::stringstream ssql;
	std::unique_lock<std::mutex> lock(mtx);
	unsigned long minFileSize;
	int rc;

	while (true) {
		cond.wait(lock);
		if(terminate == true) {
			lock.unlock();
			break;
		}

		ssql.str("");
		ssql.clear();
		ssql << "SELECT OPERATION, REQ_NUM, TARGET_STATE, NUM_REPL,"
			 << " REPL_NUM, TAPE_POOL, TAPE_ID"
			 << " FROM REQUEST_QUEUE WHERE STATE=" << DataBase::REQ_NEW
			 << " ORDER BY OPERATION,TIME_ADDED ASC;";

		sqlite3_statement::prepare(ssql.str(), &stmt);

		while ( (rc = sqlite3_statement::step(stmt)) == SQLITE_ROW ) {
			op = static_cast<DataBase::operation>(sqlite3_column_int(stmt, 0));

			/* needs to be changed/moved
			{
				std::lock_guard<std::recursive_mutex> lock(OpenLTFSInventory::mtx);
				OpenLTFSCartridge::state_t state = inventory->getCartridge(tapeId)->getState();
				if ( state == OpenLTFSCartridge::INUSE ) {
					if ( op == DataBase::SELRECALL ||
						 op == DataBase::TRARECALL) {
						suspend_map[tapeId] = true;
					}
					continue;
				}
			}
			*/

			reqNum = sqlite3_column_int(stmt, 1);
			tgtState = sqlite3_column_int(stmt, 2);
			numRepl = sqlite3_column_int(stmt, 3);
			replNum = sqlite3_column_int(stmt, 4);
			const char *pool_ctr = reinterpret_cast<const char*>(sqlite3_column_text (stmt, 5));
			if ( pool_ctr != NULL)
				pool = std::string(pool_ctr);
			else
				pool = "";
			const char *tape_id = reinterpret_cast<const char*>(sqlite3_column_text (stmt, 6));
			if (tape_id == NULL) {
				MSG(LTFSDMS0020E);
				continue;
			}
			tapeId = std::string(tape_id);

			std::lock_guard<std::recursive_mutex> lock(OpenLTFSInventory::mtx);

			if (op == DataBase::MIGRATION)
				minFileSize = smallestMigJob(reqNum, replNum);
			else
				minFileSize = 0;

			if ( resAvail(minFileSize) == false )
				continue;

			TRACE(Trace::little, reqNum);
			TRACE(Trace::little, tgtState);
			TRACE(Trace::little, numRepl);
			TRACE(Trace::little, replNum);
			TRACE(Trace::little, pool);

			std::stringstream thrdinfo;
			sqlite3_stmt *stmt3;

			switch ( op ) {
				case DataBase::MIGRATION:
					ssql.str("");
					ssql.clear();
					ssql << "UPDATE REQUEST_QUEUE SET STATE=" << DataBase::REQ_INPROGRESS
						 << " WHERE REQ_NUM=" << reqNum
						 << " AND REPL_NUM=" << replNum
						 << " AND TAPE_POOL='" << pool << "';";
					sqlite3_statement::prepare(ssql.str(), &stmt3);
					rc = sqlite3_statement::step(stmt3);
					sqlite3_statement::checkRcAndFinalize(stmt3, rc, SQLITE_DONE);

					thrdinfo << "Mig(" << reqNum << "," << replNum << "," << pool << ")";
					subs.enqueue(thrdinfo.str(), Migration::execRequest, reqNum, tgtState, numRepl, replNum, pool, tapeId, true /* needsTape */);
					break;
				case DataBase::SELRECALL:
					ssql.str("");
					ssql.clear();
					ssql << "UPDATE REQUEST_QUEUE SET STATE=" << DataBase::REQ_INPROGRESS
						 << " WHERE REQ_NUM=" << reqNum
						 << " AND TAPE_ID='" << tapeId << "';";
					sqlite3_statement::prepare(ssql.str(), &stmt3);
					rc = sqlite3_statement::step(stmt3);
					sqlite3_statement::checkRcAndFinalize(stmt3, rc, SQLITE_DONE);

					thrdinfo << "SelRec(" << reqNum << ")";
					subs.enqueue(thrdinfo.str(), SelRecall::execRequest, reqNum, tgtState, tapeId, true /* needsTape */);
					break;
				case DataBase::TRARECALL:
					ssql.str("");
					ssql.clear();
					ssql << "UPDATE REQUEST_QUEUE SET STATE=" << DataBase::REQ_INPROGRESS
						 << " WHERE REQ_NUM=" << reqNum
						 << " AND TAPE_ID='" << tapeId << "';";
					sqlite3_statement::prepare(ssql.str(), &stmt3);
					rc = sqlite3_statement::step(stmt3);
					sqlite3_statement::checkRcAndFinalize(stmt3, rc, SQLITE_DONE);

					thrdinfo << "TraRec(" << reqNum << ")";
					subs.enqueue(thrdinfo.str(), TransRecall::execRequest, reqNum, tapeId);
					break;
				default:
					TRACE(Trace::error, sqlite3_column_int(stmt, 0));
			}
		}
		sqlite3_statement::checkRcAndFinalize(stmt, rc, SQLITE_DONE);
	}
	subs.waitAllRemaining();
}
