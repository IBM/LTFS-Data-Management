#include "ServerIncludes.h"

void MessageParser::getObjects(LTFSDmCommServer *command, long localReqNumber,
							   unsigned long pid, long requestNumber, FileOperation *fopt)

{
	bool cont = true;
	bool success = true;

	TRACE(Trace::much, __PRETTY_FUNCTION__);

	while (cont) {
		try {
			command->recv();
		}
		catch(...) {
			TRACE(Trace::error, errno);
			MSG(LTFSDMS0006E);
			return;
		}

		if ( ! command->has_sendobjects() ) {
			TRACE(Trace::error, command->has_sendobjects());
			MSG(LTFSDMS0011E);
			return;
		}

		const LTFSDmProtocol::LTFSDmSendObjects sendobjects = command->sendobjects();

		DB.beginTransaction();

		for (int j = 0; j < sendobjects.filenames_size(); j++) {
			if ( terminate == true ) {
				command->closeAcc();
				return;
			}
			const LTFSDmProtocol::LTFSDmSendObjects::FileName& filename = sendobjects.filenames(j);
			if ( filename.filename().compare("") != 0 ) {
				try {
					fopt->addJob(filename.filename());
				}
				catch(int error) {
					if ( error == SQLITE_CONSTRAINT_PRIMARYKEY ||
						 error == SQLITE_CONSTRAINT_UNIQUE)
						MSG(LTFSDMS0019E, filename.filename().c_str());
					else
						MSG(LTFSDMS0015E, filename.filename().c_str(), sqlite3_errstr(error));
				}
			}
			else {
				cont = false; // END
			}
		}

		DB.endTransaction();

		LTFSDmProtocol::LTFSDmSendObjectsResp *sendobjresp = command->mutable_sendobjectsresp();

		sendobjresp->set_success(success);
		sendobjresp->set_reqnumber(requestNumber);
		sendobjresp->set_pid(pid);

		try {
			command->send();
		}
		catch(...) {
			TRACE(Trace::error, errno);
			MSG(LTFSDMS0007E);
			return;
		}
	}
}

void MessageParser::reqStatusMessage(long key, LTFSDmCommServer *command, FileOperation *fopt)

{
	long resident = 0;
	long premigrated = 0;
	long migrated = 0;
	long failed = 0;
	bool done;
	unsigned long pid;
	long requestNumber;
	long keySent;


	do {
		try {
			command->recv();
		}
		catch(...) {
			TRACE(Trace::error, errno);
			MSG(LTFSDMS0006E);
			return;
		}

		const LTFSDmProtocol::LTFSDmReqStatusRequest reqstatus = command->reqstatusrequest();

		keySent = reqstatus.key();
		if ( key != keySent ) {
			MSG(LTFSDMS0008E, keySent);
			return;
		}

		requestNumber = reqstatus.reqnumber();
		pid = reqstatus.pid();

		done = fopt->queryResult(requestNumber, &resident, &premigrated, &migrated, &failed);

		LTFSDmProtocol::LTFSDmReqStatusResp *reqstatusresp = command->mutable_reqstatusresp();

		reqstatusresp->set_success(true);
		reqstatusresp->set_reqnumber(requestNumber);
		reqstatusresp->set_pid(pid);
		reqstatusresp->set_resident(resident);
		reqstatusresp->set_premigrated(premigrated);
		reqstatusresp->set_migrated(migrated);
		reqstatusresp->set_failed(failed);
		reqstatusresp->set_done(done);

		try {
			command->send();
		}
		catch(...) {
			TRACE(Trace::error, errno);
			MSG(LTFSDMS0007E);
			return;
		}
	} while (!done);
}

void MessageParser::migrationMessage(long key, LTFSDmCommServer *command, long localReqNumber)

{
	unsigned long pid;
	long requestNumber;
	const LTFSDmProtocol::LTFSDmMigRequest migreq = command->migrequest();
	long keySent = migreq.key();

	TRACE(Trace::error, __PRETTY_FUNCTION__);
	TRACE(Trace::little, keySent);

	if ( key != keySent ) {
		MSG(LTFSDMS0008E, keySent);
		return;
	}

	requestNumber = migreq.reqnumber();
	pid = migreq.pid();

	Migration mig( pid, requestNumber, migreq.numreplica(), migreq.colfactor(), migreq.state());

	LTFSDmProtocol::LTFSDmMigRequestResp *migreqresp = command->mutable_migrequestresp();

	migreqresp->set_success(true);
	migreqresp->set_reqnumber(requestNumber);
	migreqresp->set_pid(pid);

	try {
		command->send();
	}
	catch(...) {
		TRACE(Trace::error, errno);
		MSG(LTFSDMS0007E);
		return;
	}

	getObjects(command, localReqNumber, pid, requestNumber, dynamic_cast<FileOperation*> (&mig));

	mig.addRequest();

	reqStatusMessage(key, command, dynamic_cast<FileOperation*> (&mig));
}

void  MessageParser::selRecallMessage(long key, LTFSDmCommServer *command, long localReqNumber)

{
	unsigned long pid;
	long requestNumber;
	const LTFSDmProtocol::LTFSDmSelRecRequest recreq = command->selrecrequest();
	long keySent = recreq.key();

	TRACE(Trace::error, __PRETTY_FUNCTION__);
	TRACE(Trace::little, keySent);

	if ( key != keySent ) {
		MSG(LTFSDMS0008E, keySent);
		return;
	}

	requestNumber = recreq.reqnumber();
	pid = recreq.pid();

	SelRecall srec( pid, requestNumber, recreq.state());

	LTFSDmProtocol::LTFSDmSelRecRequestResp *recreqresp = command->mutable_selrecrequestresp();

	recreqresp->set_success(true);
	recreqresp->set_reqnumber(requestNumber);
	recreqresp->set_pid(pid);

	try {
		command->send();
	}
	catch(...) {
		TRACE(Trace::error, errno);
		MSG(LTFSDMS0007E);
		return;
	}

	getObjects(command, localReqNumber, pid, requestNumber, dynamic_cast<FileOperation*> (&srec));

	srec.addRequest();

	reqStatusMessage(key, command, dynamic_cast<FileOperation*> (&srec));
}

void MessageParser::requestNumber(long key, LTFSDmCommServer *command, long *localReqNumber)

{
   	const LTFSDmProtocol::LTFSDmReqNumber reqnum = command->reqnum();
	long keySent = reqnum.key();

	TRACE(Trace::much, __PRETTY_FUNCTION__);
	TRACE(Trace::little, keySent);

	if ( key != keySent ) {
		MSG(LTFSDMS0008E, keySent);
		return;
	}

	LTFSDmProtocol::LTFSDmReqNumberResp *reqnumresp = command->mutable_reqnumresp();

	reqnumresp->set_success(true);
	*localReqNumber = ++globalReqNumber;
	reqnumresp->set_reqnumber(*localReqNumber);

	TRACE(Trace::little, *localReqNumber);

	try {
		command->send();
	}
	catch(...) {
		MSG(LTFSDMS0007E);
	}


}

void MessageParser::stopMessage(long key, LTFSDmCommServer *command, long localReqNumber)

{
   	const LTFSDmProtocol::LTFSDmStopRequest stopreq = command->stoprequest();
	long keySent = stopreq.key();

	std::unique_lock<std::mutex> lock(Scheduler::mtx);

	TRACE(Trace::error, __PRETTY_FUNCTION__);
	TRACE(Trace::little, keySent);

	if ( key != keySent ) {
		MSG(LTFSDMS0008E, keySent);
		return;
	}

	MSG(LTFSDMS0009I);

	LTFSDmProtocol::LTFSDmStopResp *stopresp = command->mutable_stopresp();

	stopresp->set_success(true);

	try {
		command->send();
	}
	catch(...) {
		MSG(LTFSDMS0007E);
	}
	command->closeRef();

	lock.unlock();
	Scheduler::cond.notify_one();
}

void MessageParser::statusMessage(long key, LTFSDmCommServer *command, long localReqNumber)

{
   	const LTFSDmProtocol::LTFSDmStatusRequest statusreq = command->statusrequest();
	long keySent = statusreq.key();

	TRACE(Trace::little, keySent);
	TRACE(Trace::error, __PRETTY_FUNCTION__);

	if ( key != keySent ) {
		MSG(LTFSDMS0008E, keySent);
		return;
	}

	LTFSDmProtocol::LTFSDmStatusResp *statusresp = command->mutable_statusresp();

	statusresp->set_success(true);

	statusresp->set_pid(getpid());

	try {
		command->send();
	}
	catch(...) {
		MSG(LTFSDMS0007E);
	}
}

void MessageParser::addMessage(long key, LTFSDmCommServer *command, long localReqNumber, Connector *connector)

{
   	const LTFSDmProtocol::LTFSDmAddRequest addreq = command->addrequest();
	long keySent = addreq.key();
	std::string managedFs = addreq.managedfs();
	std::string mountPoint = addreq.mountpoint();
	std::string fsName = addreq.fsname();
	LTFSDmProtocol::LTFSDmAddResp_AddResp response =  LTFSDmProtocol::LTFSDmAddResp::SUCCESS;

	TRACE(Trace::error, __PRETTY_FUNCTION__);
	TRACE(Trace::little, keySent);

	if ( key != keySent ) {
		MSG(LTFSDMS0008E, keySent);
		return;
	}

	try {
		FsObj fileSystem(managedFs);

		if ( fileSystem.isFsManaged() ) {
			MSG(LTFSDMS0043W, managedFs);
			response =  LTFSDmProtocol::LTFSDmAddResp::ALREADY_ADDED;
		}
		else {
			MSG(LTFSDMS0042I, managedFs);
			fileSystem.manageFs(true, connector->getStartTime(), mountPoint, fsName);
		}
	}
	catch ( int error ) {
		response = LTFSDmProtocol::LTFSDmAddResp::FAILED;
		switch ( error ) {
			case Error::LTFSDM_FS_CHECK_ERROR:
				MSG(LTFSDMS0044E, managedFs);
				break;
			case Error::LTFSDM_FS_ADD_ERROR:
				MSG(LTFSDMS0045E, managedFs);
				break;
			default:
				MSG(LTFSDMS0045E, managedFs);
		}
	}

	LTFSDmProtocol::LTFSDmAddResp *addresp = command->mutable_addresp();

	addresp->set_response(response);

	try {
		command->send();
	}
	catch(...) {
		MSG(LTFSDMS0007E);
	}
}

void MessageParser::infoRequestsMessage(long key, LTFSDmCommServer *command, long localReqNumber)

{
	const LTFSDmProtocol::LTFSDmInfoRequestsRequest inforeqs = command->inforequestsrequest();
	long keySent = inforeqs.key();
	int requestNumber = inforeqs.reqnumber();
	sqlite3_stmt *stmt;
	std::stringstream ssql;
	int rc;

	TRACE(Trace::error, __PRETTY_FUNCTION__);

	if ( key != keySent ) {
		MSG(LTFSDMS0008E, keySent);
		return;
	}

	TRACE(Trace::little, requestNumber);

	ssql << "SELECT OPERATION, REQ_NUM, TAPE_ID, TARGET_STATE, STATE FROM REQUEST_QUEUE";
	if ( requestNumber != Const::UNSET )
		ssql << " WHERE REQ_NUM=" << requestNumber << ";";
	else
		ssql << ";";

	sqlite3_statement::prepare(ssql.str(), &stmt);

	while ( (rc = sqlite3_statement::step(stmt)) == SQLITE_ROW ) {
		LTFSDmProtocol::LTFSDmInfoRequestsResp *inforeqsresp = command->mutable_inforequestsresp();

		inforeqsresp->set_operation(DataBase::opStr((DataBase::operation) sqlite3_column_int(stmt, 0)));
		inforeqsresp->set_reqnumber(sqlite3_column_int(stmt, 1));
		const char *tape_id = reinterpret_cast<const char*>(sqlite3_column_text (stmt, 2));
		if (tape_id == NULL)
			inforeqsresp->set_tapeid("");
		else
			inforeqsresp->set_tapeid(std::string(tape_id));
		inforeqsresp->set_targetstate(DataBase::reqStateStr((DataBase::req_state) sqlite3_column_int(stmt, 3)));
		inforeqsresp->set_state(DataBase::reqStateStr((DataBase::req_state) sqlite3_column_int(stmt, 4)));

		try {
			command->send();
		}
		catch(...) {
			MSG(LTFSDMS0007E);
		}
	}

	sqlite3_statement::checkRcAndFinalize(stmt, rc, SQLITE_DONE);

	LTFSDmProtocol::LTFSDmInfoRequestsResp *inforeqsresp = command->mutable_inforequestsresp();
	inforeqsresp->set_operation("");
	inforeqsresp->set_reqnumber(Const::UNSET);
	inforeqsresp->set_tapeid("");
	inforeqsresp->set_targetstate("");
	inforeqsresp->set_state("");

	try {
		command->send();
	}
	catch(...) {
		MSG(LTFSDMS0007E);
	}
}

void MessageParser::infoJobsMessage(long key, LTFSDmCommServer *command, long localReqNumber)

{
	const LTFSDmProtocol::LTFSDmInfoJobsRequest infojobs = command->infojobsrequest();
	long keySent = infojobs.key();
	int requestNumber = infojobs.reqnumber();
	sqlite3_stmt *stmt;
	std::stringstream ssql;
	int rc;

	TRACE(Trace::error, __PRETTY_FUNCTION__);

	if ( key != keySent ) {
		MSG(LTFSDMS0008E, keySent);
		return;
	}

	TRACE(Trace::little, requestNumber);

	ssql << "SELECT OPERATION, FILE_NAME, REQ_NUM, REPL_NUM, FILE_SIZE, TAPE_ID, FILE_STATE FROM JOB_QUEUE";
	if ( requestNumber != Const::UNSET )
		ssql << " WHERE REQ_NUM=" << requestNumber << ";";
	else
		ssql << ";";

	sqlite3_statement::prepare(ssql.str(), &stmt);

	while ( (rc = sqlite3_statement::step(stmt)) == SQLITE_ROW ) {
		LTFSDmProtocol::LTFSDmInfoJobsResp *infojobsresp = command->mutable_infojobsresp();

		infojobsresp->set_operation(DataBase::opStr((DataBase::operation) sqlite3_column_int(stmt, 0)));
		const char *file_name = reinterpret_cast<const char*>(sqlite3_column_text (stmt, 1));
		if (file_name == NULL)
			infojobsresp->set_filename("-");
		else
			infojobsresp->set_filename(std::string(file_name));
		infojobsresp->set_reqnumber(sqlite3_column_int(stmt, 2));
		infojobsresp->set_replnumber(sqlite3_column_int(stmt, 3));
		infojobsresp->set_filesize(sqlite3_column_int64(stmt, 4));
		const char *tape_id = reinterpret_cast<const char*>(sqlite3_column_text (stmt, 5));
		if (tape_id == NULL)
			infojobsresp->set_tapeid("-");
		else
			infojobsresp->set_tapeid(std::string(tape_id));
		infojobsresp->set_state(FsObj::migStateStr((FsObj::file_state) sqlite3_column_int(stmt, 6)));

		try {
			command->send();
		}
		catch(...) {
			MSG(LTFSDMS0007E);
		}
	}

	sqlite3_statement::checkRcAndFinalize(stmt, rc, SQLITE_DONE);

	LTFSDmProtocol::LTFSDmInfoJobsResp *infojobsresp = command->mutable_infojobsresp();
	infojobsresp->set_operation("");
	infojobsresp->set_filename("");
	infojobsresp->set_reqnumber(Const::UNSET);
	infojobsresp->set_replnumber(Const::UNSET);
	infojobsresp->set_filesize(Const::UNSET);
	infojobsresp->set_tapeid("");
	infojobsresp->set_state("");

	try {
		command->send();
	}
	catch(...) {
		MSG(LTFSDMS0007E);
	}
}

void MessageParser::infoDrivesMessage(long key, LTFSDmCommServer *command)

{
	const LTFSDmProtocol::LTFSDmInfoDrivesRequest infodrives = command->infodrivesrequest();
	long keySent = infodrives.key();

	TRACE(Trace::error, __PRETTY_FUNCTION__);

	if ( key != keySent ) {
		MSG(LTFSDMS0008E, keySent);
		return;
	}

	inventory->lock();

	for (OpenLTFSDrive i : inventory->getDrives()) {
		LTFSDmProtocol::LTFSDmInfoDrivesResp *infodrivesresp = command->mutable_infodrivesresp();

		infodrivesresp->set_id(i.GetObjectID());
		infodrivesresp->set_devname(i.get_devname());
		infodrivesresp->set_slot(i.get_slot());
		infodrivesresp->set_status(i.get_status());
		infodrivesresp->set_busy(i.isBusy());

		try {
			command->send();
		}
		catch(...) {
			MSG(LTFSDMS0007E);
		}
	}

	inventory->unlock();

	LTFSDmProtocol::LTFSDmInfoDrivesResp *infodrivesresp = command->mutable_infodrivesresp();

	infodrivesresp->set_id("");
	infodrivesresp->set_devname("");
	infodrivesresp->set_slot(0);
	infodrivesresp->set_status("");
	infodrivesresp->set_busy(false);

	try {
		command->send();
	}
	catch(...) {
		MSG(LTFSDMS0007E);
	}
}

void MessageParser::infoTapesMessage(long key, LTFSDmCommServer *command)

{
	const LTFSDmProtocol::LTFSDmInfoTapesRequest infotapes = command->infotapesrequest();
	long keySent = infotapes.key();
	std::string state;

	TRACE(Trace::error, __PRETTY_FUNCTION__);

	if ( key != keySent ) {
		MSG(LTFSDMS0008E, keySent);
		return;
	}

	inventory->lock();

	for (OpenLTFSCartridge i : inventory->getCartridges()) {
		LTFSDmProtocol::LTFSDmInfoTapesResp *infotapesresp = command->mutable_infotapesresp();

		infotapesresp->set_id(i.GetObjectID());
		infotapesresp->set_slot(i.get_slot());
		infotapesresp->set_totalcap(i.get_total_cap());
		infotapesresp->set_remaincap(i.get_remaining_cap());
		infotapesresp->set_status(i.get_status());
		infotapesresp->set_inprogress(i.getInProgress());
		infotapesresp->set_pool(i.getPool());
		switch ( i.getState() ) {
			case OpenLTFSCartridge::INUSE: state = messages[LTFSDMS0055I]; break;
			case OpenLTFSCartridge::MOUNTED: state = messages[LTFSDMS0056I]; break;
			case OpenLTFSCartridge::MOVING: state = messages[LTFSDMS0057I]; break;
			case OpenLTFSCartridge::UNMOUNTED: state = messages[LTFSDMS0058I]; break;
			case OpenLTFSCartridge::INVALID: state = messages[LTFSDMS0059I]; break;
			case OpenLTFSCartridge::UNKNOWN: state = messages[LTFSDMS0060I]; break;
			default: state = "-";
		}

		infotapesresp->set_state(state);

		try {
			command->send();
		}
		catch(...) {
			MSG(LTFSDMS0007E);
		}
	}

	inventory->unlock();

	LTFSDmProtocol::LTFSDmInfoTapesResp *infotapesresp = command->mutable_infotapesresp();

	infotapesresp->set_id("");
	infotapesresp->set_slot(0);
	infotapesresp->set_totalcap(0);
	infotapesresp->set_remaincap(0);
	infotapesresp->set_status("");
	infotapesresp->set_inprogress(0);
	infotapesresp->set_pool("");
	infotapesresp->set_status("");

	try {
		command->send();
	}
	catch(...) {
		MSG(LTFSDMS0007E);
	}
}

void MessageParser::run(long key, LTFSDmCommServer command, Connector *connector)

{
	std::unique_lock<std::mutex> lock(Server::termmtx);
	bool firstTime = true;
	long localReqNumber = Const::UNSET;

	while (true) {
		try {
			command.recv();
		}
		catch(...) {
			TRACE(Trace::error, errno);
			MSG(LTFSDMS0006E);
			lock.unlock();
			Server::termcond.notify_one();
			return;
		}

		TRACE(Trace::much, "new message received");

		if ( command.has_reqnum() ) {
			requestNumber(key, &command, &localReqNumber);
		}
		else if ( command.has_stoprequest() ) {
			stopMessage(key, &command, localReqNumber);
			terminate = true;
			lock.unlock();
			Server::termcond.notify_one();
			kill(getpid(), SIGUSR1);
			break;
		}
		else {
			if ( firstTime ) {
				lock.unlock();
				Server::termcond.notify_one();
				firstTime = false;
			}
			if ( command.has_migrequest() ) {
				migrationMessage(key, &command, localReqNumber);
			}
			else if ( command.has_selrecrequest() ) {
				selRecallMessage(key, &command, localReqNumber);
			}
			else if ( command.has_statusrequest() ) {
				statusMessage(key, &command, localReqNumber);
			}
			else if ( command.has_addrequest() ) {
				addMessage(key, &command, localReqNumber, connector);
			}
			else if ( command.has_inforequestsrequest() ) {
				infoRequestsMessage(key, &command, localReqNumber);
			}
			else if ( command.has_infojobsrequest() ) {
				infoJobsMessage(key, &command, localReqNumber);
			}
			else if ( command.has_infodrivesrequest() ) {
				infoDrivesMessage(key, &command);
			}
			else if ( command.has_infotapesrequest() ) {
				infoTapesMessage(key, &command);
			}
			else {
				TRACE(Trace::error, "unkown command\n");
			}
			break;
		}
	}
	command.closeAcc();
}
