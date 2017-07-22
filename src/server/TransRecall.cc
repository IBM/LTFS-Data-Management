#include "ServerIncludes.h"

void TransRecall::addRequest(Connector::rec_info_t recinfo, std::string tapeId, long reqNum)

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

	ssql << "INSERT INTO JOB_QUEUE (OPERATION, FILE_NAME, REQ_NUM, TARGET_STATE, REPL_NUM, FILE_SIZE, FS_ID, I_GEN, "
		 << "I_NUM, MTIME_SEC, MTIME_NSEC, LAST_UPD, FILE_STATE, TAPE_ID, START_BLOCK, CONN_INFO) "
		 << "VALUES (" << DataBase::TRARECALL << ", "            // OPERATION
		 << filename.c_str() << ", "                  // FILE_NAME
		 << reqNum << ", "                                       // REQ_NUM
		 << ( recinfo.toresident ?
			  FsObj::RESIDENT : FsObj::PREMIGRATED ) << ", "    // TARGET_STATE
		 << Const::UNSET << ", ";                               // REPL_NUM needed since trec requests
	try {                                                       // are not deleted immeditely
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
		ssql << Scheduler::getStartBlock(tapeName) << ", "       // START_BLOCK
			 << (std::intptr_t) recinfo.conn_info << ");";       // CONN_INFO
	}
	catch ( const std::exception& e ) {
		TRACE(Trace::error, e.what());
		if ( filename.compare("NULL") != 0 )
			MSG(LTFSDMS0073E, filename);
		else
			MSG(LTFSDMS0032E, recinfo.ino);
	}

	TRACE(Trace::normal, ssql.str());

	sqlite3_statement::prepare(ssql.str(), &stmt);
	rc = sqlite3_statement::step(stmt);
	sqlite3_statement::checkRcAndFinalize(stmt, rc, SQLITE_DONE);

	if ( filename.compare("NULL") != 0 )
		TRACE(Trace::always, filename);
	else
		TRACE(Trace::always, recinfo.ino);

	TRACE(Trace::always, tapeId);

	std::unique_lock<std::mutex> lock(Scheduler::mtx);

	bool reqExists = false;
	ssql.str("");
	ssql.clear();

	ssql << "SELECT STATE FROM REQUEST_QUEUE WHERE REQ_NUM=" << reqNum;
	sqlite3_statement::prepare(ssql.str(), &stmt);
	while ( (rc = sqlite3_statement::step(stmt)) == SQLITE_ROW )
		reqExists = true;
	sqlite3_statement::checkRcAndFinalize(stmt, rc, SQLITE_DONE);

	if ( reqExists == true ) {
		ssql.str("");
		ssql.clear();
		ssql << "UPDATE REQUEST_QUEUE SET STATE=" << DataBase::REQ_NEW
			 << " WHERE REQ_NUM=" << reqNum << " AND TAPE_ID='" << tapeId << "';";
		TRACE(Trace::normal, ssql.str());
		sqlite3_statement::prepare(ssql.str(), &stmt);
		rc = sqlite3_statement::step(stmt);
		sqlite3_statement::checkRcAndFinalize(stmt, rc, SQLITE_DONE);
		Scheduler::cond.notify_one();
	}
	else {
		ssql.str("");
		ssql.clear();
		ssql << "INSERT INTO REQUEST_QUEUE (OPERATION, REQ_NUM, "
			 << "TAPE_ID, TIME_ADDED, STATE) "
			 << "VALUES (" << DataBase::TRARECALL << ", "                          // OPERATION
			 << reqNum << ", "                                                     // REQ_NUMR
			 << "'" << attr.tapeId[0] << "', "                                     // TAPE_ID
			 << time(NULL) << ", "                                                 // TIME_ADDED
			 << DataBase::REQ_NEW << ");";                                         // STATE
		TRACE(Trace::normal, ssql.str());
		sqlite3_statement::prepare(ssql.str(), &stmt);
		rc = sqlite3_statement::step(stmt);
		sqlite3_statement::checkRcAndFinalize(stmt, rc, SQLITE_DONE);

		Scheduler::cond.notify_one();
	}
}

void TransRecall::run(Connector *connector)

{
	SubServer subs(Const::MAX_TRANSPARENT_RECALL_THREADS);
	Connector::rec_info_t recinfo;
	std::map<std::string, long> reqmap;
	std::set<std::string> fsList;
	std::set<std::string>::iterator it;

	try {
		connector->initTransRecalls();
	}
	catch ( const std::exception& e ) {
		TRACE(Trace::error, e.what());
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
		catch ( const OpenLTFSException& e ) {
			TRACE(Trace::error, e.what());
			switch ( e.getError() ) {
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
		catch( const std::exception& e ) {
			TRACE(Trace::error, e.what());
		}
	}

	while (Connector::connectorTerminate == false) {
		try {
			recinfo = connector->getEvents();
		}
		catch ( const std::exception& e ) {
			MSG(LTFSDMS0036W, e.what());
		}

		// is sent for termination
		if ( recinfo.conn_info == NULL ) {
			TRACE(Trace::always, recinfo.ino);
			continue;
		}

		if ( Server::terminate == true ) {
			TRACE(Trace::always, (bool) Server::terminate);
			connector->respondRecallEvent(recinfo, false);
			continue;
		}

		if ( recinfo.ino == 0 ) {
			TRACE(Trace::always, recinfo.ino);
			continue;
		}

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
		catch ( const OpenLTFSException& e ) {
			TRACE(Trace::error, e.what());
			if ( e.getError() == Error::LTFSDM_ATTR_FORMAT )
				MSG(LTFSDMS0037W, recinfo.ino);
			else
				MSG(LTFSDMS0038W, recinfo.ino, e.getError());
			connector->respondRecallEvent(recinfo, false);
			continue;
		}
		catch ( const std::exception& e ) {
			TRACE(Trace::error, e.what());
			connector->respondRecallEvent(recinfo, false);
			continue;
		}


		std::string tapeId = fso.getAttribute().tapeId[0];

		std::stringstream thrdinfo;
		thrdinfo << "TrRec(" << recinfo.ino << ")";

		if ( reqmap.count(tapeId) == 0 )
			reqmap[tapeId] = ++globalReqNumber;

		TRACE(Trace::always, recinfo.ino);
		TRACE(Trace::always, tapeId);
		TRACE(Trace::always, reqmap[tapeId]);

		subs.enqueue(thrdinfo.str(), TransRecall::addRequest, recinfo, tapeId, reqmap[tapeId]);
	}

	MSG(LTFSDMS0083I);
	subs.waitAllRemaining();
	MSG(LTFSDMS0084I);
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

		TRACE(Trace::always, recinfo.ino);
		TRACE(Trace::always, recinfo.filename);

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
				throw(EXCEPTION(Const::UNSET, tapeName, errno));
			}

			statbuf = target.stat();

			target.prepareRecall();

			while ( offset < statbuf.st_size ) {
				if ( Server::forcedTerminate )
					throw(EXCEPTION(Const::UNSET, tapeName));

				rsize = read(fd, buffer, sizeof(buffer));
				if ( rsize == -1 ) {
					TRACE(Trace::error, errno);
					MSG(LTFSDMS0023E, tapeName.c_str());
					throw(EXCEPTION(Const::UNSET, tapeName, errno));
				}
				wsize = target.write(offset, (unsigned long) rsize, buffer);
				if ( wsize != rsize ) {
					TRACE(Trace::error, errno);
					TRACE(Trace::error, wsize);
					TRACE(Trace::error, rsize);
					MSG(LTFSDMS0033E, recinfo.ino);
					close(fd);
					throw(EXCEPTION(Const::UNSET, recinfo.ino, wsize, rsize));
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
		TRACE(Trace::error, e.what());
		if ( fd != -1 )
			close(fd);
		throw(EXCEPTION(Const::UNSET));
	}

	return statbuf.st_size;
}


void recallStep(int reqNum, std::string tapeId)

{
	Connector::rec_info_t recinfo;
	sqlite3_stmt *stmt;
	std::stringstream ssql;
	int rc;
	FsObj::file_state state;
	FsObj::file_state toState;
	struct respinfo_t {
		Connector::rec_info_t recinfo;
		bool succeeded;
	};
	std::list<respinfo_t> resplist;
	int numFiles = 0;
	bool succeeded;

	ssql << "UPDATE JOB_QUEUE SET FILE_STATE=" <<  FsObj::RECALLING_MIG
		 << " WHERE REQ_NUM=" << reqNum
		 << " AND FILE_STATE=" << FsObj::MIGRATED
		 << " AND TAPE_ID='" << tapeId << "'";

	TRACE(Trace::normal, ssql.str());
	sqlite3_statement::prepare(ssql.str(), &stmt);
	rc = sqlite3_statement::step(stmt);
	sqlite3_statement::checkRcAndFinalize(stmt, rc, SQLITE_DONE);

	ssql.str("");
	ssql.clear();

	ssql << "UPDATE JOB_QUEUE SET FILE_STATE=" <<  FsObj::RECALLING_PREMIG
		 << " WHERE REQ_NUM=" << reqNum
		 << " AND FILE_STATE=" << FsObj::PREMIGRATED
		 << " AND TAPE_ID='" << tapeId << "'";

	TRACE(Trace::normal, ssql.str());
	sqlite3_statement::prepare(ssql.str(), &stmt);
	rc = sqlite3_statement::step(stmt);
	sqlite3_statement::checkRcAndFinalize(stmt, rc, SQLITE_DONE);

	ssql.str("");
	ssql.clear();

	ssql << "SELECT FS_ID, I_GEN, I_NUM, FILE_NAME, FILE_STATE, TARGET_STATE, CONN_INFO  FROM JOB_QUEUE "
		 << "WHERE REQ_NUM=" << reqNum
		 << " AND (FILE_STATE=" << FsObj::RECALLING_MIG << " OR FILE_STATE=" << FsObj::RECALLING_PREMIG << ")"
		 << " AND TAPE_ID='" << tapeId
		 << "' ORDER BY START_BLOCK";
	TRACE(Trace::normal, ssql.str());

	sqlite3_statement::prepare(ssql.str(), &stmt);

	while ( (rc = sqlite3_statement::step(stmt) ) ) {
		if ( rc != SQLITE_ROW )
			break;

		numFiles++;

		recinfo = (Connector::rec_info_t) {0, 0, 0, 0, 0, ""};
		recinfo.fsid = (unsigned long long) sqlite3_column_int64(stmt, 0);
		recinfo.igen = (unsigned int) sqlite3_column_int(stmt, 1);
		recinfo.ino = (unsigned long long) sqlite3_column_int64(stmt, 2);
		const char *cstr = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));
		if ( cstr != NULL )
			recinfo.filename = std::string(cstr);
		state = (FsObj::file_state) sqlite3_column_int(stmt, 4);
		if ( state ==  FsObj::RECALLING_MIG )
			state = FsObj::MIGRATED;
		else
			state = FsObj::PREMIGRATED;
		toState = (FsObj::file_state) sqlite3_column_int(stmt, 5);
		if ( toState == FsObj::RESIDENT )
			recinfo.toresident = true;
		recinfo.conn_info = (struct conn_info_t *) sqlite3_column_int64(stmt, 6);

		TRACE(Trace::always, recinfo.filename);
		TRACE(Trace::always, recinfo.ino);
		TRACE(Trace::always, state);
		TRACE(Trace::always, toState);

		try {
			recall(recinfo, tapeId, state, toState);
			succeeded = true;
		}
		catch(const std::exception& e) {
			TRACE(Trace::error, e.what());
			succeeded = false;
		}

		TRACE(Trace::always, succeeded);
		resplist.push_back((respinfo_t) {recinfo, succeeded});
	}

	TRACE(Trace::always, numFiles);

	sqlite3_statement::checkRcAndFinalize(stmt, rc, SQLITE_DONE);

	ssql.str("");
	ssql.clear();

	ssql << "DELETE FROM JOB_QUEUE "
		 << "WHERE REQ_NUM=" << reqNum
		 << " AND (FILE_STATE=" << FsObj::RECALLING_MIG << " OR FILE_STATE=" << FsObj::RECALLING_PREMIG << ")"
		 << " AND TAPE_ID='" << tapeId << "'";
 	TRACE(Trace::normal, ssql.str());

	sqlite3_statement::prepare(ssql.str(), &stmt);
	rc = sqlite3_statement::step(stmt);
	sqlite3_statement::checkRcAndFinalize(stmt, rc, SQLITE_DONE);

	for ( respinfo_t respinfo : resplist )
		Connector::respondRecallEvent(respinfo.recinfo, respinfo.succeeded);
}


void TransRecall::execRequest(int reqNum, std::string tapeId)

{
	std::stringstream ssql;
	sqlite3_stmt *stmt;
	int remaining = 0;
	int rc;

	recallStep(reqNum, tapeId);

	std::unique_lock<std::mutex> lock(Scheduler::mtx);

	{
		std::lock_guard<std::recursive_mutex> inventorylock(OpenLTFSInventory::mtx);
		inventory->getCartridge(tapeId)->setState(OpenLTFSCartridge::MOUNTED);
		bool found = false;
		for ( std::shared_ptr<OpenLTFSDrive> d : inventory->getDrives() ) {
			if ( d->get_slot() == inventory->getCartridge(tapeId)->get_slot() ) {
				TRACE(Trace::normal, d->GetObjectID());
				d->setFree();
				found = true;
				break;
			}
		}
		assert(found == true);
	}

	ssql.str("");
	ssql.clear();
	ssql << "SELECT COUNT(*) FROM JOB_QUEUE WHERE REQ_NUM="
		 << reqNum << " AND TAPE_ID='" << tapeId << "';";
 	TRACE(Trace::normal, ssql.str());
	sqlite3_statement::prepare(ssql.str(), &stmt);
	while ( (rc = sqlite3_statement::step(stmt)) == SQLITE_ROW )
				remaining = sqlite3_column_int(stmt, 0);
	sqlite3_statement::checkRcAndFinalize(stmt, rc, SQLITE_DONE);

	ssql.str("");
	ssql.clear();
	if ( remaining )
		ssql << "UPDATE REQUEST_QUEUE SET STATE=" << DataBase::REQ_NEW
			 << ", TIME_ADDED=" << time(NULL)
			 << " WHERE REQ_NUM=" << reqNum << " AND TAPE_ID='" << tapeId << "';";
	else
		ssql << "DELETE FROM REQUEST_QUEUE"
			 << " WHERE REQ_NUM=" << reqNum << " AND TAPE_ID='" << tapeId << "';";
 	TRACE(Trace::normal, ssql.str());
	sqlite3_statement::prepare(ssql.str(), &stmt);
	rc = sqlite3_statement::step(stmt);
	sqlite3_statement::checkRcAndFinalize(stmt, rc, SQLITE_DONE);
	Scheduler::cond.notify_one();
}
