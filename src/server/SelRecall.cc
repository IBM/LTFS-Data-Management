#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include <iostream>
#include <sstream>

#include <sqlite3.h>
#include "src/common/comm/ltfsdm.pb.h"

#include "src/common/util/util.h"
#include "src/common/messages/Message.h"
#include "src/common/tracing/Trace.h"
#include "src/common/errors/errors.h"
#include "src/common/const/Const.h"

#include "src/connector/Connector.h"

#include "DataBase.h"
#include "FileOperation.h"

#include "SelRecall.h"

void SelRecall::addFileName(std::string fileName)

{
	int rc;
	struct stat statbuf;
	std::stringstream ssql;
	FsObj *fso = NULL;

	ssql << "INSERT INTO JOB_QUEUE (OPERATION, FILE_NAME, REQ_NUM, TARGET_STATE, COLOC_NUM, FILE_SIZE, FS_ID, I_GEN, I_NUM, MTIME, LAST_UPD, FAILED) ";
	ssql << "VALUES (" << DataBase::SELRECALL << ", ";            // OPERATION
	ssql << "'" << fileName << "', ";                             // FILE_NAME
	ssql << reqNumber << ", ";                                    // REQ_NUM
	ssql << targetState << ", ";                                  // MIGRATION_STATE
	ssql << Const::UNSET << ", ";                                 // COLOC_NUM

	try {
		fso = new FsObj(fileName);
		statbuf = fso->stat();
	}
	catch ( int error ) {
		MSG(LTFSDMS0017E, fileName.c_str());
		goto failed;
	}

	if (!S_ISREG(statbuf.st_mode)) {
		MSG(LTFSDMS0018E, fileName.c_str());
		goto failed;
	}

	ssql << statbuf.st_size << ", ";                              // FILE_SIZE

	try {
		ssql << fso->getFsId() << ", ";                           // FS_ID
		ssql << fso->getIGen() << ", ";                           // I_GEN
		ssql << fso->getINode() << ", ";                          // I_NUM
	}
	catch ( int error ) {
		MSG(LTFSDMS0017E, fileName.c_str());
		goto failed;
	}

	ssql << statbuf.st_mtime << ", ";                             // MTIME
	ssql << time(NULL) << ", ";                                   // LAST_UPD
	ssql << 0 << ");";                                            // FAILED

	if ( fso )
		delete(fso);

	rc = sqlite3_prepare_v2(DB.getDB(), ssql.str().c_str(), -1, &stmt, NULL);

	if( rc != SQLITE_OK ) {
		std::cout << "rc: " << rc << std::endl;
		TRACE(Trace::error, rc);
		throw(LTFSDMErr::LTFSDM_GENERAL_ERROR);
	}

	rc = sqlite3_step(stmt);

	if ( rc != SQLITE_DONE ) {
		TRACE(Trace::error, rc);
		throw(LTFSDMErr::LTFSDM_GENERAL_ERROR);
	}

	rc = sqlite3_finalize(stmt);

	if ( rc != SQLITE_OK ) {
		TRACE(Trace::error, rc);
		throw(LTFSDMErr::LTFSDM_GENERAL_ERROR);
	}
	return;

failed:
	if ( fso )
		delete(fso);
}

void SelRecall::start()

{
	//	int i = 0;

	std::cout << "Selective Recall request" << std::endl;

	std::cout << "pid: " << pid << std::endl;
	std::cout << "request number: " << reqNumber << std::endl;

	switch (targetState) {
		case LTFSDmProtocol::LTFSDmSelRecRequest::RESIDENT:
			std::cout << "files to be recalled to resident state" << std::endl;
			break;
		case LTFSDmProtocol::LTFSDmSelRecRequest::PREMIGRATED:
			std::cout << "files to be recalled to premigrated state" << std::endl;
			break;
		default:
			std::cout << "unkown target state" << std::endl;
	}

	// for(std::vector<std::string>::iterator it = fileList.begin(); it != fileList.end(); ++it) {
	// 	std::cout << "file " << ++i << ": " << *it << std::endl;
	// }

	std::cout << std::endl;

}
