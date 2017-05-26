#include "ServerIncludes.h"

std::mutex Scheduler::mtx;
std::condition_variable Scheduler::cond;
std::mutex Scheduler::updmtx;
std::condition_variable Scheduler::updcond;
std::map<int, std::atomic<bool>> Scheduler::updReq;
std::map<std::string, std::atomic<bool>> Scheduler::suspend_map;


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

bool Scheduler::poolResAvail()

{
	bool found;

	assert(pool.compare("") != 0 );

	for ( std::shared_ptr<OpenLTFSCartridge> card : inventory->getPool(pool)->getCartridges() ) {
		if ( card->getState() == OpenLTFSCartridge::MOUNTED ) {
			tapeId = card->GetObjectID();
			found = false;
			for ( std::shared_ptr<OpenLTFSDrive> drive : inventory->getDrives() ) {
				if ( drive->get_slot() == card->get_slot() ) {
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
	}

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
			for ( std::shared_ptr<OpenLTFSCartridge> card : inventory->getPool(pool)->getCartridges() ) {
				if ( card->getState() == OpenLTFSCartridge::UNMOUNTED ) {
					TRACE(Trace::always, std::string("SET BUSY: ") + drive->GetObjectID());
					drive->setBusy();
					card->setState(OpenLTFSCartridge::MOVING);
					std::thread t(mount, drive->GetObjectID(), card->GetObjectID());
					t.detach();
					return false;
				}
			}
		}
	}

	for ( std::shared_ptr<OpenLTFSDrive> drive : inventory->getDrives() ) {
		if ( drive->isBusy() == true )
			continue;
		for ( std::shared_ptr<OpenLTFSCartridge> card : inventory->getCartridges() ) {
			if ( (drive->get_slot() == card->get_slot()) &&
				 (card->getState() == OpenLTFSCartridge::MOUNTED)) {
				TRACE(Trace::always, std::string("SET BUSY: ") + drive->GetObjectID());
				drive->setBusy();
				card->setState(OpenLTFSCartridge::MOVING);
				std::thread t(unmount, drive->GetObjectID(), card->GetObjectID());
				t.detach();
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

	if ( inventory->getCartridge(tapeId)->getState() == OpenLTFSCartridge::MOUNTED ) {
		found = false;
		for ( std::shared_ptr<OpenLTFSDrive> drive : inventory->getDrives() ) {
			if ( drive->get_slot() == inventory->getCartridge(tapeId)->get_slot() ) {
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
				inventory->getCartridge(tapeId)->setState(OpenLTFSCartridge::MOVING);
				std::thread t(mount, drive->GetObjectID(), tapeId);
				t.detach();
				return false;
			}
		}
	}

	for ( std::shared_ptr<OpenLTFSDrive> drive : inventory->getDrives() ) {
		if ( drive->isBusy() == true )
			continue;
		for ( std::shared_ptr<OpenLTFSCartridge> card : inventory->getCartridges() ) {
			if ( (drive->get_slot() == card->get_slot()) &&
				 (card->getState() == OpenLTFSCartridge::MOUNTED)) {
				TRACE(Trace::always, std::string("SET BUSY: ") + drive->GetObjectID());
				drive->setBusy();
				card->setState(OpenLTFSCartridge::MOVING);
				std::thread t(unmount, drive->GetObjectID(), card->GetObjectID());
				t.detach();
				return false;
			}
		}
	}

	return false;
}


bool Scheduler::resAvail()

{
	if ( op == DataBase::MIGRATION )
		return poolResAvail();
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


void Scheduler::run(long key)

{
	TRACE(Trace::much, __PRETTY_FUNCTION__);
	sqlite3_stmt *stmt;
	std::stringstream ssql;
	std::unique_lock<std::mutex> lock(mtx);
	SubServer subs;
	int rc;

	// std::vector<std::string> tapeIds = LTFSDM::getTapeIds();
	// for(auto const& tapeId: tapeIds)
	// 	suspend_map[tapeId] = false;

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
			 << " ORDER BY OPERATION,REQ_NUM;";

		sqlite3_statement::prepare(ssql.str(), &stmt);

		while ( (rc = sqlite3_statement::step(stmt)) == SQLITE_ROW ) {
			op = static_cast<DataBase::operation>(sqlite3_column_int(stmt, 0));

			/* needs to be changed/moved
			{
				std::lock_guard<std::mutex> lock(inventory->mtx);
				OpenLTFSCartridge::state_t state = inventory->getCartridge(tapeId)->getState();
				if ( state == OpenLTFSCartridge::INUSE ) {
					if ( op == DataBase::SELRECALL ||
						 op == DataBase::TRARECALL) {
						suspend_map[tapeId] = true;
					}
					continue;
				}
				inventory->getCartridge(tapeId)->setState(OpenLTFSCartridge::INUSE);
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

			std::lock_guard<std::mutex> lock(inventory->mtx);

			if ( resAvail() == false )
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
