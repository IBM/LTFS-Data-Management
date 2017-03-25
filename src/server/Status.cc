#include <sys/resource.h>

#include <iostream>
#include <sstream>
#include <string>
#include <map>
#include <set>
#include <vector>
#include <mutex>

#include <sqlite3.h>

#include "src/common/util/util.h"
#include "src/common/messages/Message.h"
#include "src/common/tracing/Trace.h"
#include "src/common/errors/errors.h"
#include "src/common/const/Const.h"

#include "src/server/DataBase.h"
#include "src/connector/Connector.h"

#include "src/server/Status.h"

Status  mrStatus;

void Status::add(int reqNumber)

{
	std::stringstream ssql;
	sqlite3_stmt *stmt;
	int rc;

	ssql << "SELECT FILE_STATE, COUNT(*) FROM JOB_QUEUE WHERE REQ_NUM="
		 << reqNumber << " GROUP BY FILE_STATE";

	sqlite3_statement::prepare(ssql.str(), &stmt);

	singleState state;

	while ( (rc = sqlite3_statement::step(stmt)) == SQLITE_ROW ) {
		switch ( sqlite3_column_int(stmt, 0) ) {
			case FsObj::RESIDENT:
			case FsObj::PREMIGRATING:
				state.resident = sqlite3_column_int(stmt, 1);
				break;
			case FsObj::PREMIGRATED:
			case FsObj::STUBBING:
			case FsObj::RECALLING_PREMIG:
				state.premigrated = sqlite3_column_int(stmt, 1);
				break;
			case FsObj::MIGRATED:
			case FsObj::RECALLING_MIG:
				state.migrated = sqlite3_column_int(stmt, 1);
				break;
			case FsObj::FAILED:
				state.failed = sqlite3_column_int(stmt, 1);
				break;
			default:
				TRACE(Trace::error, sqlite3_column_int(stmt, 0));
		}
	}

	try {
		sqlite3_statement::checkRcAndFinalize(stmt, rc, SQLITE_DONE);
	}
	catch ( int error ) {
		TRACE(Trace::error, error);
		state = singleState();
	}

	allStates[reqNumber] = state;
}


void Status::remove(int reqNumber)

{
	std::lock_guard<std::mutex> lock(Status::mtx);

	allStates.erase(reqNumber);
}

void Status::updateSuccess(int reqNumber, FsObj::file_state from, FsObj::file_state to)

{
	std::lock_guard<std::mutex> lock(Status::mtx);

	if ( allStates.count(reqNumber) == 0 )
		add(reqNumber);

	singleState state = allStates[reqNumber];

	switch (from) {
		case FsObj::RESIDENT:
			state.resident--;
			break;
		case FsObj::PREMIGRATED:
			state.premigrated--;
			break;
		case FsObj::MIGRATED:
			state.migrated--;
			break;
		default:
			break;
	}

	switch (to) {
		case FsObj::RESIDENT:
			state.resident++;
			break;
		case FsObj::PREMIGRATED:
			state.premigrated++;
			break;
		case FsObj::MIGRATED:
			state.migrated++;
			break;
		default:
			break;
	}

	allStates[reqNumber] = state;
}

void Status::updateFailed(int reqNumber, FsObj::file_state from)

{
	std::lock_guard<std::mutex> lock(Status::mtx);

	if ( allStates.count(reqNumber) == 0 )
		add(reqNumber);

	singleState state = allStates[reqNumber];

	switch (from) {
		case FsObj::RESIDENT:
			state.resident--;
			break;
		case FsObj::PREMIGRATED:
			state.premigrated--;
			break;
		case FsObj::MIGRATED:
			state.migrated--;
			break;
		default:
			break;
	}

	state.failed++;

	allStates[reqNumber] = state;
}

void Status::get(int reqNumber, long *resident, long *premigrated, long *migrated, long *failed)

{
	std::lock_guard<std::mutex> lock(Status::mtx);

	if ( allStates.count(reqNumber) == 0 )
		add(reqNumber);

	*resident = allStates[reqNumber].resident;
	*premigrated = allStates[reqNumber].premigrated;
	*migrated = allStates[reqNumber].migrated;
	*failed = allStates[reqNumber].failed;
}
