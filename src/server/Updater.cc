#include <string>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include <map>
#include <chrono>

#include "src/connector/Connector.h"
#include "Updater.h"


void MigrationUpdater::setUpdate(int colGrp, int end, FsObj::file_state state)

{
    std::unique_lock<std::mutex> lock(MigrationUpdater::updmutex);
	if ( statusMap[colGrp].start = Const::UNSET )
		statusMap[colGrp].start = end;
	statusMap[colGrp].end = end;
	statusMap[colGrp].state = state;
}

void MigrationUpdater::setStart(int colGrp, int start)

{
	std::unique_lock<std::mutex> lock(MigrationUpdater::updmutex);
	statusMap[colGrp].start = start;
}

MigrationUpdater::updateInfo MigrationUpdater::getUpdate(int colGrp)

{
	std::unique_lock<std::mutex> lock(MigrationUpdater::updmutex);
	return statusMap[colGrp];
}

void MigrationUpdater::run()

{
	sqlite3_stmt *stmt;
	std::stringstream ssql;
	std::unique_lock<std::mutex> slck(Scheduler::updmtx);
	std::unique_lock<std::mutex> lock(MigrationUpdater::updmutex);

	while ( !MigrationUpdater::done ) {
		updcond.wait_for(lock, std::chrono::seconds(10), [](){return  MigrationUpdater::done == true;});
		for (std::map<int, updateInfo>::iterator it=statusMap.begin(); it!=statusMap.end(); ++it) {
			ssql.str("");
			ssql.clear();

			ssql << "UPDATE JOB_QUEUE SET FILE_STATE = " <<  it->second.state;
			ssql << " WHERE REQ_NUM=" << reqNum << " AND COLOC_GRP = " << it->first;
			ssql << " AND (ROWID BETWEEN " << it->second.start << " AND " << it->second.end << ")";

			slck.lock();

			sqlite3_statement::prepare(ssql.str(), &stmt);
			rc2 = sqlite3_statement::step(stmt);
			sqlite3_statement::checkRcAndFinalize(stmt, rc2, SQLITE_DONE);

			it->second.start = Const::UNSET;

			Scheduler::updReq = reqNum;
			Scheduler::updcond.notify_all();
			slck.unlock();
		}
	}
}




void RecallUpdater::setUpdate(std::string tapeId, int end, FsObj::file_state state)

{
	std::unique_lock<std::mutex> lock(RecallUpdater::updmutex);
	statusMap[tapeId].end = end;
	statusMap[tapeId].state = state;
}

void RecallUpdater::setStart(std::string tapeId, int start)

{
	std::unique_lock<std::mutex> lock(RecallUpdater::updmutex);
	statusMap[tapeId].start = start;
}

RecallUpdater::updateInfo RecallUpdater::getUpdate(std::string tapeId)

{
	std::unique_lock<std::mutex> lock(RecallUpdater::updmutex);
	return statusMap[tapeId];
}

void RecallUpdater::run()

{
	std::unique_lock<std::mutex> lock(RecallUpdater::updmutex);
	while ( !RecallUpdater::done ) {
		updcond.wait_for(lock, std::chrono::seconds(10), [](){return RecallUpdater::done == true;});
		for (std::map<std::string, updateInfo>::iterator it=statusMap.begin(); it!=statusMap.end(); ++it) {

		}
	}
}
