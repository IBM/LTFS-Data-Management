#include "ServerIncludes.h"

/* ======== Scheduler ======== */

const std::string Scheduler::SELECT_REQUEST =
	"SELECT OPERATION, REQ_NUM, TARGET_STATE, NUM_REPL,"
	" REPL_NUM, TAPE_POOL, TAPE_ID"
	" FROM REQUEST_QUEUE WHERE STATE=%1%"
	" ORDER BY OPERATION,TIME_ADDED ASC";

const std::string Scheduler::UPDATE_MIG_REQUEST =
	"UPDATE REQUEST_QUEUE SET STATE=%1%"
	" WHERE REQ_NUM=%2%"
	" AND REPL_NUM=%3%"
	" AND TAPE_POOL='%4%'";

const std::string Scheduler::UPDATE_REC_REQUEST =
	"UPDATE REQUEST_QUEUE SET STATE=%1%"
	" WHERE REQ_NUM=%2%"
	" AND TAPE_ID='%3%'";

const std::string Scheduler::SMALLEST_MIG_JOB =
	"SELECT MIN(FILE_SIZE) FROM JOB_QUEUE WHERE"
	" REQ_NUM=%1%"
	" AND FILE_STATE=%2%"
	" AND REPL_NUM=%3%";

/* ======== Migration ======== */

const std::string Migration::ADD_MIGRATION_JOB =
	"INSERT INTO JOB_QUEUE (OPERATION, FILE_NAME, REQ_NUM, TARGET_STATE, REPL_NUM, TAPE_POOL, "
	"FILE_SIZE, FS_ID, I_GEN, I_NUM, MTIME_SEC, MTIME_NSEC, LAST_UPD, TAPE_ID, FILE_STATE) "
	"VALUES (" /* OPERATION */ "%1%, " /* FILE_NAME */ "'%2%', " /* REQ_NUM */ "%3%, "
	/* MIGRATION_STATE */ "%4%, " /* REPL_NUM */ "?, " /* TAPE_POOL */"?, "
	/* FILE_SIZE */ "%5%, " /* FS_ID */ "%6%, " /* I_GEN */ "%7%, " /* I_NUM */ "%8%, "
	/* MTIME_SEC */ "%9%, " /* MTIME_NSEC */ "%10%, " /* LAST_UPD */ "%11%, "
	/* TAPE_ID */ "'', " /* FILE_STATE */ "%12%)";

const  std::string Migration::ADD_MIGRATION_REQUEST =
	"INSERT INTO REQUEST_QUEUE (OPERATION, REQ_NUM, TARGET_STATE, "
	"NUM_REPL, REPL_NUM, TAPE_POOL, TAPE_ID, TIME_ADDED, STATE) "
	"VALUES (" /* OPERATION */  "%1%, " /* FILE_NAME */ "%2%, " /* TARGET_STATE */ "%3%, "
	/* NUM_REPL */  "%4%, " /* REPL_NUM */  "%5%, " /* TAPE_POOL */ "'%6%', "
	/* TAPE_ID */ "'', " /* TIME_ADDED */ "%7%, " /* STATE */ "%8%);";

const std::string Migration::FAIL_PREMIGRATION =
	"UPDATE JOB_QUEUE SET FILE_STATE=%1%"
	" WHERE REQ_NUM=%2%"
	" AND FILE_NAME='%3%'"
	" AND REPL_NUM=%4%";

const std::string Migration::FAIL_STUBBING =
	"UPDATE JOB_QUEUE SET FILE_STATE=%1%"
	" WHERE REQ_NUM=%2%"
	" AND FILE_NAME='%3%'";

const std::string Migration::SET_PREMIGRATING =
	"UPDATE JOB_QUEUE SET FILE_STATE=%1%"
	", TAPE_ID='%2%'"
	" WHERE REQ_NUM=%3%"
	" AND FILE_STATE=%4%"
	" AND REPL_NUM=%5%"
	" AND FITS(I_NUM, FILE_SIZE, %6%, %7%, %8%)=1";

const std::string Migration::SET_STUBBING =
	"UPDATE JOB_QUEUE SET FILE_STATE=%1%"
	" WHERE REQ_NUM=%2%"
	" AND FILE_STATE=%3%"
	" AND TAPE_ID='%4%'"
	" AND REPL_NUM=%5%";

const std::string Migration::SELECT_MIG_JOBS =
	"SELECT FILE_NAME, MTIME_SEC, MTIME_NSEC, I_NUM FROM JOB_QUEUE WHERE"
	" REQ_NUM=%1%"
	" AND FILE_STATE=%2%"
	" AND TAPE_ID='%3%'";

const std::string Migration::SET_MIG_SUCCESS =
	"UPDATE JOB_QUEUE SET FILE_STATE=%1%"
	" WHERE REQ_NUM=%2%"
	" AND FILE_STATE=%3%"
	" AND TAPE_ID='%4%'"
	" AND I_NUM IN (%5%)";

const std::string Migration::SET_MIG_RESET =
	"UPDATE JOB_QUEUE SET FILE_STATE=%1%"
	" WHERE REQ_NUM=%2%"
	" AND FILE_STATE=%3%";

const std::string Migration::FAIL_PREMIGRATED =
	"UPDATE JOB_QUEUE SET FILE_STATE=%1%"
	" WHERE REQ_NUM=%2%"
	" AND FILE_STATE=%3%"
	" AND TAPE_ID='%4%'"
	" AND REPL_NUM=%5%";

const std::string Migration::UPDATE_MIG_REQUEST =
	"UPDATE REQUEST_QUEUE SET STATE=%1%"
	" WHERE REQ_NUM=%2%"
	" AND REPL_NUM=%3%";

const std::string Migration::UPDATE_MIG_REQUEST_RESET_TAPE =
	"UPDATE REQUEST_QUEUE SET STATE=%1%, TAPE_ID=''"
	" WHERE REQ_NUM=%2%"
	" AND REPL_NUM=%3%";

/* ======== SelRecall ======== */

const std::string SelRecall::ADD_SELRECALL_JOB =
	"INSERT INTO JOB_QUEUE (OPERATION, FILE_NAME, REQ_NUM, TARGET_STATE, FILE_SIZE, FS_ID, I_GEN, "
	"I_NUM, MTIME_SEC, MTIME_NSEC, LAST_UPD, FILE_STATE, TAPE_ID, START_BLOCK) "
	"VALUES (" /* OPERATION */ "%1%, " /* FILE_NAME */ "'%2%', " /* REQ_NUM */ "%3%, "
	/* MIGRATION_STATE */ "%4%, " /* FILE_SIZE */ "%5%, " /* FS_ID */ "%6%, "
	/* I_GEN */ "%7%, " /* I_NUM */ "%8%, " /* MTIME_SEC */ "%9%, " /* MTIME_NSEC */ "%10%, "
	/* LAST_UPD */"%11%, " /* FILE_STATE */ "%12%, " /* TAPE_ID */ "'%13%', " /* START_BLOCK */ "%14%)";

const std::string SelRecall::GET_TAPES =
	"SELECT TAPE_ID FROM JOB_QUEUE WHERE REQ_NUM=%1%"
	" GROUP BY TAPE_ID";

const std::string SelRecall::ADD_SELRECALL_REQUEST =
	"INSERT INTO REQUEST_QUEUE (OPERATION, REQ_NUM, TARGET_STATE, TAPE_ID, TIME_ADDED, STATE) "
	"VALUES (" /* OPERATION */ "%1%, " /* REQ_NUM */ "%2%, " /* TARGET_STATE */ "%3%, "
	/* TAPE_ID */ "'%4%', " /* TIME_ADDED */ "%5%, " /* STATE */ "%6%)";

const std::string SelRecall::SET_RECALLING =
	"UPDATE JOB_QUEUE SET FILE_STATE=%1%"
	" WHERE REQ_NUM=%2%"
	" AND FILE_STATE=%3%"
	" AND TAPE_ID='%4%'";

const std::string SelRecall::SELECT_REC_JOBS =
	"SELECT FILE_NAME, FILE_STATE, I_NUM FROM JOB_QUEUE WHERE REQ_NUM=%1%"
	" AND TAPE_ID='%2%'"
	" AND (FILE_STATE=%3% OR FILE_STATE=%4%)"
	" ORDER BY START_BLOCK";

const std::string SelRecall::FAIL_RECALL =
	"UPDATE JOB_QUEUE SET FILE_STATE = %1%"
	" WHERE FILE_NAME='%2%'"
	" AND REQ_NUM=%3%"
	" AND TAPE_ID='%4%'";

const std::string SelRecall::SET_REC_SUCCESS =
	"UPDATE JOB_QUEUE SET FILE_STATE = %1%"
	" WHERE REQ_NUM=%2%"
	" AND TAPE_ID='%3%'"
	" AND (FILE_STATE=%4% OR FILE_STATE=%5%)"
	" AND I_NUM IN (%6%)";

const std::string SelRecall::SET_REC_RESET =
	"UPDATE JOB_QUEUE SET FILE_STATE = %1%"
	" WHERE REQ_NUM=%2%"
	" AND TAPE_ID='%3%'"
	" AND FILE_STATE=%4%";

const std::string SelRecall::UPDATE_REQ_REQUEST =
	"UPDATE REQUEST_QUEUE SET STATE=%1%"
	" WHERE REQ_NUM=%2%"
	" AND TAPE_ID='%3%';";

/* ======== ======== */
/* ======== ======== */
/* ======== ======== */
/* ======== ======== */
