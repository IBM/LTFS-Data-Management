#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>
#include <sys/resource.h>
#include <errno.h>

#include <string>
#include <sstream>
#include <atomic>
#include <typeinfo>
#include <map>
#include <set>
#include <vector>
#include <mutex>

#include "src/common/util/util.h"
#include "src/common/messages/Message.h"
#include "src/common/tracing/Trace.h"
#include "src/common/errors/errors.h"
#include "src/common/const/Const.h"

#include <xfs/dmapi.h>

#include "src/connector/Connector.h"

std::atomic<dm_sessid_t> dmapiSession;
std::atomic<dm_token_t> dmapiToken;
std::atomic<bool> Connector::connectorTerminate(false);

struct conn_info_t {
	conn_info_t(dm_token_t _token) : token(_token) {}
	dm_token_t token;
};

typedef struct {
	unsigned long long fsid;
	unsigned int igen;
	unsigned long long ino;
} fuid_t;

struct ltstr
{
	bool operator()(const fuid_t fuid1, const fuid_t fuid2) const
	{
        return ( fuid1.ino < fuid2.ino )
			|| (( fuid1.ino == fuid2.ino )
				&& (( fuid1.igen < fuid2.igen )
					|| (( fuid1.igen == fuid2.igen )
						&& (( fuid1.fsid < fuid2.fsid )))));
	}
};

std::map<fuid_t, int, ltstr> fuidMap;

std::mutex mtx;

void dmapiSessionCleanup(dm_sessid_t *oldSid)

{
	int i;
	unsigned int j;
	unsigned int num_sessions = 0;
	unsigned int num_sessions_res = 0;
	dm_sessid_t *sidbufp = NULL;
    unsigned long rseslenp;
	unsigned int rtoklenp;
    char buffer[DM_SESSION_INFO_LEN];

	sidbufp = (dm_sessid_t*)malloc(sizeof( dm_sessid_t ));

	if ( sidbufp == NULL ) {
		MSG(LTFSDMD0001E);
		throw(Error::LTFSDM_GENERAL_ERROR);
	}

	while ( dm_getall_sessions(num_sessions, sidbufp, &num_sessions_res) == -1 ) {
        free(sidbufp);

        if ( errno != E2BIG ) {
            TRACE(Trace::error, errno);
			throw(Error::LTFSDM_GENERAL_ERROR);
        }

		sidbufp = NULL;
        sidbufp = (dm_sessid_t*)malloc(num_sessions_res * sizeof( dm_sessid_t ));

		if ( !sidbufp ) {
            MSG(LTFSDMD0001E);
			throw(Error::LTFSDM_GENERAL_ERROR);
        }

		num_sessions = num_sessions_res;
    }

	if ( num_sessions )
		MSG(LTFSDMD0002I, num_sessions);

	unsigned int num_tokens = 1024;
	dm_token_t *tokbufp = NULL;

	for ( i = 0; i < (int)num_sessions; i++ ) {
        if ( dm_query_session(sidbufp[i], sizeof( buffer ), buffer, &rseslenp) == -1 ) {
            MSG(LTFSDMD0001E);
			free(sidbufp);
			throw(Error::LTFSDM_GENERAL_ERROR);
        }
		else if (Const::DMAPI_SESSION_NAME.compare(buffer) == 0) {
			TRACE(Trace::little, i);
			TRACE(Trace::little, (unsigned long) sidbufp[i]);

			tokbufp =  (dm_token_t *) malloc(sizeof(dm_token_t) * num_tokens);
			if ( !tokbufp ) {
				MSG(LTFSDMD0001E);
				free(sidbufp);
				throw(Error::LTFSDM_GENERAL_ERROR);
			}

			while (dm_getall_tokens(sidbufp[i], num_tokens, tokbufp, &rtoklenp) == -1) {
				if ( errno != E2BIG ) {
					TRACE(Trace::error, errno);
					throw(Error::LTFSDM_GENERAL_ERROR);
				}
				free(tokbufp);
				tokbufp = NULL;
				tokbufp =  (dm_token_t *) malloc(sizeof(dm_token_t) * rtoklenp);
				if ( !tokbufp ) {
					MSG(LTFSDMD0001E);
					free(sidbufp);
					throw(Error::LTFSDM_GENERAL_ERROR);
				}
			}

			TRACE(Trace::little, rtoklenp);

			for (j = 0; j<rtoklenp; j++)
			{
				TRACE(Trace::little, j);
				TRACE(Trace::little, (unsigned long) tokbufp[j]);
				if ( dm_respond_event(sidbufp[i], tokbufp[j], DM_RESP_ABORT, EINTR, 0, NULL) == 1 )
					TRACE(Trace::error, errno);
				else
					MSG(LTFSDMD0003I);
			}

			if ( dm_destroy_session(sidbufp[i]) == -1 ) {
				TRACE(Trace::error, errno);
				MSG(LTFSDMD0004E);
				*oldSid = sidbufp[i];
			}
			else {
				MSG(LTFSDMD0005I, (unsigned long) sidbufp[i]);
			}
		}
	}
	if (sidbufp != NULL)
		free(sidbufp);
	if (tokbufp != NULL)
		free(tokbufp);
}

Connector::Connector(bool cleanup)

{
	char          *version          = NULL;
    size_t         msglen           = 8;
    char           msgdatap[8];
	dm_sessid_t    dmapiSessionLoc;
	dm_token_t     dmapiTokenLoc;
	dm_sessid_t    oldSid = DM_NO_SESSION;

	memset((char *) msgdatap, 0, sizeof(msgdatap));

    if (dm_init_service(&version)) {
		TRACE(Trace::error, errno);
		goto failed;
	}

	if ( cleanup )
		dmapiSessionCleanup(&oldSid);

	if (dm_create_session(oldSid, (char *) Const::DMAPI_SESSION_NAME.c_str(), &dmapiSessionLoc)) {
		TRACE(Trace::error, errno);
		goto failed;
	}

    if(dm_create_userevent(dmapiSessionLoc, msglen, (void*)msgdatap, &dmapiTokenLoc)) {
        dm_destroy_session(dmapiSessionLoc);
		TRACE(Trace::error, errno);
		goto failed;
	}

	dmapiSession = dmapiSessionLoc;
	dmapiToken = dmapiTokenLoc;
	return;

failed:
	MSG(LTFSDMS0016E);
	throw(Error::LTFSDM_GENERAL_ERROR);
}

Connector::~Connector()

{
	if ( dm_respond_event(dmapiSession, dmapiToken, DM_RESP_ABORT, EINTR, 0, NULL) == 1 )
		TRACE(Trace::error, errno);

	dm_destroy_session(dmapiSession);
}

void recoverDisposition()

{
	int rc = 0;
	size_t bufLen = 4096;
	size_t rLen;
	char *bufP = NULL;
	dm_dispinfo_t *dispP;
	void *hp;
	size_t hlen;
	size_t mountBufLen = 4096;
	size_t mlen;
	char *mountBufP = NULL;
	dm_mount_event_t *mountEventP;
	dm_eventset_t eventSet;
	void *hand1P;
	size_t hand1Len;
	char *name1P;
	char *name2P;
	size_t name1Len;
	size_t name2Len;
	char sgName[256], nameBuf[256];

retry:
	/* Allocate buffer space */
	bufP =  (char *)malloc( bufLen );
	if (bufP == NULL) {
		MSG(LTFSDMD0001E);
		throw(Error::LTFSDM_GENERAL_ERROR);
	}

	/* get all prior dispositions */
	rc = dm_getall_disp(dmapiSession, bufLen, bufP, &rLen);
	if (rc != 0) {
		if (errno == E2BIG) {
			bufLen = rLen;
			free (bufP);
			bufP = NULL;
			goto retry;
		}
		TRACE(Trace::error, errno);
		goto exit;
	}
	if (rLen == 0)
		goto exit;

	/* Parse returned buffer */
	for (dispP = (dm_dispinfo_t *) bufP; dispP != NULL;
		 dispP = DM_STEP_TO_NEXT(dispP, dm_dispinfo_t *)) {

		/* get fs handle */
		hp = DM_GET_VALUE(dispP, di_fshandle, u_char *);
		hlen = DM_GET_LEN(dispP, di_fshandle);

		/* There are no dispositions if there is no handle */
		if (dm_handle_is_valid(hp, hlen) == DM_FALSE)
			break;

retry2:
		/* Allocate buffer for mount info for this disposition */
		if (mountBufP == NULL) {
			mountBufP =  (char *)malloc( mountBufLen );
			if (mountBufP == NULL) {
				rc = 1;
				TRACE(Trace::error, errno);
				goto exit;
			}
		}

		/* Recover mount information for this disposition */
		rc = dm_get_mountinfo(dmapiSession, hp, hlen, DM_NO_TOKEN,
							  mountBufLen, mountBufP, &mlen);
		if (rc != 0) {
			if (errno == E2BIG) {
				mountBufLen = mlen;
				free (mountBufP);
				mountBufP = NULL;
				goto retry2;
			}
			if ((errno == EBADF) ||
				(errno == EINVAL)) {
				rc = 0;
				continue;
			}
			else if (errno == EPERM) {
				rc = 0;
				continue;
			}
			TRACE(Trace::error, errno);
			goto exit;
		}

		mountEventP = (dm_mount_event_t *)mountBufP;
		hand1P = DM_GET_VALUE(mountEventP , me_handle1, void *);
		hand1Len = DM_GET_LEN(mountEventP, me_handle1);
		name1P = DM_GET_VALUE(mountEventP, me_name1, char *);
		name1Len = DM_GET_LEN(mountEventP, me_name1);
		name2P = DM_GET_VALUE(mountEventP, me_name2, char *);
		name2Len = DM_GET_LEN(mountEventP, me_name2);

		if ((name1Len >= sizeof(nameBuf)) ||
			(name2Len >= sizeof(sgName))) {
			rc = -1;
			TRACE(Trace::error, name1Len);
			TRACE(Trace::error, name2Len);
			goto exit;
		}

		memcpy(nameBuf, name1P, name1Len);
		nameBuf[name1Len] = '\0';
		memcpy(sgName, name2P, name2Len);
		sgName[name2Len] = '\0';

		/* Set dm disposition for events. */
		DMEV_ZERO(eventSet);
		DMEV_SET(DM_EVENT_READ, eventSet);
		DMEV_SET(DM_EVENT_WRITE, eventSet);
		DMEV_SET(DM_EVENT_TRUNCATE, eventSet);

		rc = dm_set_disp(dmapiSession, hand1P, hand1Len,
						 DM_NO_TOKEN, &eventSet, DM_EVENT_MAX);
		if (rc < 0) {
			TRACE(Trace::error, errno);
			goto exit;
		}
	}

exit:

	if (bufP != NULL)
		free (bufP);
	if (mountBufP != NULL)
		free (mountBufP);
	if ( rc != 0 ) {
		MSG(LTFSDMD0006E);
		throw(Error::LTFSDM_GENERAL_ERROR);
	}
}


void Connector::initTransRecalls()

{
	dm_eventset_t eventSet;

	recoverDisposition();

	/* Register for mount events */
	DMEV_ZERO(eventSet);
	DMEV_SET(DM_EVENT_MOUNT, eventSet);
	if ( dm_set_disp(dmapiSession, DM_GLOBAL_HANP, DM_GLOBAL_HLEN, DM_NO_TOKEN,
					 &eventSet, DM_EVENT_MAX) == -1 ) {
		TRACE(Trace::error, errno);
		throw(Error::LTFSDM_GENERAL_ERROR);
	}
}

Connector::rec_info_t Connector::getEvents()

{
	rec_info_t recinfo;
	dm_eventset_t eventSet;
	char eventBuf[8192];
	dm_eventmsg_t *eventMsgP;
	dm_mount_event_t *mountEventP;
	dm_data_event_t *dataEventP;
	char *msg;
	unsigned int msglen;
	char nameBuf[PATH_MAX];
	char sgName[PATH_MAX];
	void *hand1P;
	size_t hand1Len;
	void *handRP;
	size_t handRLen;
	char *name1P;
	char *name2P;
	size_t name1Len;
	size_t name2Len;
	bool toresident;
	size_t rlen;
	dm_token_t token;
	int retries;

	recinfo = (Connector::rec_info_t) {0, 0, 0, 0, 0, ""};

	while ( dm_get_events(dmapiSession, 1, DM_EV_WAIT, sizeof(eventBuf), eventBuf, &rlen) == -1 ) {
		TRACE(Trace::error, errno);
		if ( errno != EINTR && errno != EAGAIN )
			throw(Error::LTFSDM_GENERAL_ERROR);
	}

    /* Process event */
    eventMsgP = (dm_eventmsg_t *) eventBuf;
    token = eventMsgP->ev_token;

	TRACE(Trace::little, eventMsgP->ev_type);

	switch (eventMsgP->ev_type)
	{
        case DM_EVENT_MOUNT:
			mountEventP = DM_GET_VALUE(eventMsgP, ev_data, dm_mount_event_t*);
			hand1P = DM_GET_VALUE(mountEventP , me_handle1, void *);
			hand1Len = DM_GET_LEN(mountEventP, me_handle1);
			handRP = DM_GET_VALUE(mountEventP , me_roothandle, void *);
			handRLen = DM_GET_LEN(mountEventP, me_roothandle);
			name1P = DM_GET_VALUE(mountEventP, me_name1, char *);
			name1Len = DM_GET_LEN(mountEventP, me_name1);
			name2P = DM_GET_VALUE(mountEventP, me_name2, char *);
			name2Len = DM_GET_LEN(mountEventP, me_name2);

			if ((name1Len >= sizeof(nameBuf)) ||
				(name2Len >= sizeof(sgName)))
			{
				TRACE(Trace::error, name1Len);
				TRACE(Trace::error, name2Len);
				throw(Error::LTFSDM_GENERAL_ERROR);
			}

			memcpy(nameBuf, name1P, name1Len);
			nameBuf[name1Len] = '\0';
			memcpy(sgName, name2P, name2Len);
			sgName[name2Len] = '\0';

			TRACE(Trace::little, nameBuf);
			TRACE(Trace::little, sgName);

			MSG(LTFSDMD0009I, nameBuf);
			MSG(LTFSDMD0010I, nameBuf);

			DMEV_ZERO(eventSet);
			DMEV_SET(DM_EVENT_READ, eventSet);
			DMEV_SET(DM_EVENT_WRITE, eventSet);
			DMEV_SET(DM_EVENT_TRUNCATE, eventSet);

			if ( dm_set_disp(dmapiSession, hand1P, hand1Len, DM_NO_TOKEN, &eventSet, DM_EVENT_MAX) == -1 ) {
				TRACE(Trace::error, errno);
				throw(Error::LTFSDM_GENERAL_ERROR);
			}

			/* Respond w/ continue to indicate we are managing this fs */
			if ( dm_respond_event(dmapiSession, token, DM_RESP_CONTINUE, 0, 0, NULL) == -1 ) {
				TRACE(Trace::error, errno);
				throw(Error::LTFSDM_GENERAL_ERROR);
			}

			retries = 0;

			while ( retries < 4 ) {
				try {
					FsObj fileSystem(handRP, handRLen);
					fileSystem.manageFs(false, starttime);
					break;
				}
				catch ( int error ) {
					switch ( error ) {
						case Error::LTFSDM_FS_ADD_ERROR:
							usleep(100000);
							retries++;
							break;
						default:
							usleep(100000);
							retries++;
					}
				}
			}

			if ( retries == 4 )
				MSG(LTFSDMD0011E, nameBuf);
			TRACE(Trace::little, retries);

			break;  /* end of case DM_EVENT_MOUNT */
		case DM_EVENT_READ:
		case DM_EVENT_WRITE:
		case DM_EVENT_TRUNCATE:
			/* Determine the type of recall */
			if (eventMsgP->ev_type == DM_EVENT_READ)
				toresident = false;
			else
				toresident = true;

			/* Get a handle to the file being accessed */
			dataEventP = DM_GET_VALUE(eventMsgP, ev_data,
									  dm_data_event_t *);
			hand1P = DM_GET_VALUE(dataEventP, de_handle, void *);
			hand1Len = DM_GET_LEN(dataEventP, de_handle);

			/* Recall file data on-line */
			/*
			rc = handleDataEvents(eventMsgP->ev_type, sid, token, coresidentRecall,
								  hand1P, hand1Len,
								  dataEventP->de_offset, dataEventP->de_length);
			*/
			recinfo.toresident = toresident;
			recinfo.conn_info = new conn_info_t(token);

			if (dm_handle_to_fsid(hand1P, hand1Len, &recinfo.fsid)) {
				TRACE(Trace::error, errno);
				throw(Error::LTFSDM_GENERAL_ERROR);
			}

			if (dm_handle_to_igen(hand1P, hand1Len, &recinfo.igen)) {
				TRACE(Trace::error, errno);
				throw(Error::LTFSDM_GENERAL_ERROR);
			}

			if (dm_handle_to_ino(hand1P, hand1Len, &recinfo.ino)) {
				TRACE(Trace::error, errno);
				throw(Error::LTFSDM_GENERAL_ERROR);
			}

			break;
		case DM_EVENT_USER:
			msg = DM_GET_VALUE(eventMsgP, ev_data, char *);
			msglen = DM_GET_LEN(eventMsgP, ev_data);
			msg[msglen] = 0;
			MSG(LTFSDMD0008I, msg);
			break;
		default:
			TRACE(Trace::error, eventMsgP->ev_type);
			break;

	} /* end of switch on all event types */

	return recinfo;
}

void Connector::respondRecallEvent(rec_info_t recinfo, bool success)

{
	dm_token_t token = recinfo.conn_info->token;

	if ( success == true ) {
		if ( dm_respond_event(dmapiSession, token, DM_RESP_CONTINUE, 0, 0, NULL) == -1 ) {
			TRACE(Trace::error, errno);
			throw(Error::LTFSDM_GENERAL_ERROR);
		}
	}
	else {
		if ( dm_respond_event(dmapiSession, token, DM_RESP_ABORT, EIO, 0, NULL) == -1 ) {
			TRACE(Trace::error, errno);
			throw(Error::LTFSDM_GENERAL_ERROR);
		}
	}

	TRACE(Trace::little, recinfo.ino);
}

void Connector::terminate()

{
	if ( dm_send_msg(dmapiSession, DM_MSGTYPE_ASYNC,
					 Const::DMAPI_TERMINATION_MESSAGE.size(),
					 (void *) Const::DMAPI_TERMINATION_MESSAGE.c_str()) == -1 ) {
		TRACE(Trace::error, errno);
		MSG(LTFSDMD0007E);
	}

	Connector::connectorTerminate = true;
}

FsObj::FsObj(std::string fileName)
	: handle(NULL), handleLength(0), isLocked(false), handleFree(true)

{
	char *fname = (char *) fileName.c_str();

	if ( dm_path_to_handle(fname, &handle, &handleLength) != 0 ) {
		TRACE(Trace::error, errno);
		throw(errno);
	}
}

FsObj::FsObj(Connector::rec_info_t recinfo)
	: handle(NULL), handleLength(0), isLocked(false), handleFree(true)

{
	if ( dm_make_handle(&recinfo.fsid, &recinfo.ino, &recinfo.igen, &handle, &handleLength) != 0 ) {
		TRACE(Trace::error, errno);
		throw(errno);
	}
}

FsObj::~FsObj()

{
	if ( handleFree )
	dm_handle_free(handle, handleLength);
}

bool FsObj::isFsManaged()

{
	unsigned long rsize;
	FsObj::fs_attr_t attr;
	int rc;

	memset(&attr, 0, sizeof(fs_attr_t));

    rc = dm_get_dmattr(dmapiSession, handle, handleLength, DM_NO_TOKEN,
					   (dm_attrname_t *) Const::DMAPI_ATTR_FS.c_str(), sizeof(attr), &attr, &rsize);


	if ( rc == -1 && errno == ENOENT ) {
		return false;
	}
	else if ( rc == -1 ) {
		TRACE(Trace::error, errno);
		throw(Error::LTFSDM_FS_CHECK_ERROR);
	}

	return attr.managed;
}

void FsObj::manageFs(bool setDispo, struct timespec starttime, std::string mountPoint, std::string fsName)

{
	FsObj::fs_attr_t attr;
	dm_eventset_t eventSet;
	memset(&attr, 0, sizeof(fs_attr_t));
	attr.managed = true;
	void *fsHandle = NULL;
    unsigned long fsHandleLength;

	lock();
	if ( dm_set_dmattr(dmapiSession, handle, handleLength,
					   dmapiToken, (dm_attrname_t *) Const::DMAPI_ATTR_FS.c_str(),
					   0, sizeof(fs_attr_t), (void *) &attr) == -1 ) {
		unlock();
		TRACE(Trace::error, errno);
		throw(Error::LTFSDM_FS_ADD_ERROR);
	}
	unlock();

	if ( setDispo ) {
		DMEV_ZERO(eventSet);
		DMEV_SET(DM_EVENT_READ, eventSet);
		DMEV_SET(DM_EVENT_WRITE, eventSet);
		DMEV_SET(DM_EVENT_TRUNCATE, eventSet);

		try {
			if ( dm_handle_to_fshandle(handle, handleLength, &fsHandle, &fsHandleLength) == -1 ) {
				TRACE(Trace::error, handle);
				TRACE(Trace::error, fsHandle);
				TRACE(Trace::error, errno);
				throw(Error::LTFSDM_FS_ADD_ERROR);
			}
			if ( dm_set_disp(dmapiSession, fsHandle, fsHandleLength, DM_NO_TOKEN, &eventSet, DM_EVENT_MAX) == -1 ) {
				dm_handle_free(fsHandle, fsHandleLength);
				TRACE(Trace::error, errno);
				throw(Error::LTFSDM_FS_ADD_ERROR);
			}
		}
		catch ( int error ) {
			lock();
			attr.managed = false;
			if ( dm_set_dmattr(dmapiSession, handle, handleLength,
							   dmapiToken, (dm_attrname_t *) Const::DMAPI_ATTR_FS.c_str(),
							   0, sizeof(fs_attr_t), (void *) &attr) == -1 ) {
				unlock();
				TRACE(Trace::error, errno);
				throw(Error::LTFSDM_FS_ADD_ERROR);
			}
			unlock();
			throw(Error::LTFSDM_FS_ADD_ERROR);
		}
		dm_handle_free(fsHandle, fsHandleLength);
	}
}

struct stat FsObj::stat()

{
	struct stat statbuf;
	dm_stat_t dmstatbuf;

	memset(&statbuf, 0, sizeof(statbuf));

	if ( handle == NULL )
		return statbuf;

 	if ( dm_get_fileattr(dmapiSession, handle, handleLength,
						 DM_NO_TOKEN, DM_AT_STAT, &dmstatbuf) != 0 ) {
		TRACE(Trace::error, errno);
	    throw(errno);
	}

	statbuf.st_dev = dmstatbuf.dt_dev;
	statbuf.st_ino = dmstatbuf.dt_ino;
	statbuf.st_mode = dmstatbuf.dt_mode;
	statbuf.st_nlink = dmstatbuf.dt_nlink;
	statbuf.st_uid = dmstatbuf.dt_uid;
	statbuf.st_gid = dmstatbuf.dt_gid;
	statbuf.st_rdev = dmstatbuf.dt_rdev;
	statbuf.st_size = dmstatbuf.dt_size;
	statbuf.st_blksize = dmstatbuf.dt_blksize;
	statbuf.st_blocks = dmstatbuf.dt_blocks;
	statbuf.st_atime = dmstatbuf.dt_atime;
	statbuf.st_mtime = dmstatbuf.dt_mtime;
	statbuf.st_ctime = dmstatbuf.dt_ctime;

	return statbuf;
}

unsigned long long FsObj::getFsId()

{
	unsigned long long fsid;

	if ( handleLength == 0 )
		return 0;

	if (dm_handle_to_fsid(handle, handleLength, &fsid)) {
		TRACE(Trace::error, errno);
	    throw(errno);
	}

	return fsid;
}

unsigned int FsObj::getIGen()

{
	unsigned int igen;

	if ( handleLength == 0 )
		return 0;

	if (dm_handle_to_igen(handle, handleLength, &igen)) {
		TRACE(Trace::error, errno);
	    throw(errno);
	}

	return igen;
}

unsigned long long FsObj::getINode()

{
	unsigned long long ino;

	if ( handleLength == 0 )
		return 0;

	if (dm_handle_to_ino(handle, handleLength, &ino)) {
		TRACE(Trace::error, errno);
	    throw(errno);
	}

	return ino;
}

std::string FsObj::getTapeId()

{
	std::stringstream sstr;

	sstr << "DV148" << random() % 2 <<  "L6";

	return sstr.str();
}

void FsObj::lock()

{
	int rc;
	fuid_t fuid = (fuid_t) {getFsId(), getIGen(), getINode()};
	std::stringstream sstream;

	std::unique_lock<std::mutex> lock(mtx);

	if ( fuidMap.count(fuid) == 0 ) {
		rc = dm_request_right(dmapiSession, handle, handleLength,
							  dmapiToken, DM_RR_WAIT, DM_RIGHT_EXCL);

		if ( rc == -1 ) {
			TRACE(Trace::error, errno);
			throw(errno);
		}

		fuidMap[fuid] = 1;
		sstream << "new(" << fuidMap[fuid] << "): " << fuid.fsid << ", " << fuid.igen << ", " << fuid.ino;
		TRACE(Trace::much, sstream.str());
	}
	else {
		fuidMap[fuid]++;
		sstream << "inc(" << fuidMap[fuid] << "): " << fuid.fsid << ", " << fuid.igen << ", " << fuid.ino;
		TRACE(Trace::much, sstream.str());
	}

	isLocked = true;
}

void FsObj::unlock()

{
	int rc;
	fuid_t fuid = (fuid_t) {getFsId(), getIGen(), getINode()};
	std::stringstream sstream;

	std::unique_lock<std::mutex> lock(mtx);

	if ( isLocked == false ) {
		TRACE(Trace::error, isLocked);
		throw(Error::LTFSDM_GENERAL_ERROR);
	}

	if  ( fuidMap.count(fuid) != 1 ) {
		TRACE(Trace::error, fuidMap.count(fuid));
		TRACE(Trace::error, fuid.fsid);
		TRACE(Trace::error, fuid.igen);
		TRACE(Trace::error, fuid.ino);
	}

	assert ( fuidMap.count(fuid) == 1 );

	if ( fuidMap[fuid] == 1 ) {
		rc = dm_release_right(dmapiSession, handle, handleLength, dmapiToken);

		if ( rc == -1 ) {
			TRACE(Trace::error, errno);
			throw(errno);
		}

		fuidMap.erase(fuid);
		sstream << "rem(" << fuidMap.count(fuid) << "): " << fuid.fsid << ", " << fuid.igen << ", " << fuid.ino;
		TRACE(Trace::much, sstream.str());
	}
	else {
		fuidMap[fuid]--;
		sstream << "dec(" << fuidMap[fuid] << "): " << fuid.fsid << ", " << fuid.igen << ", " << fuid.ino;
		TRACE(Trace::much, sstream.str());
	}

	isLocked = false;
}

long FsObj::read(long offset, unsigned long size, char *buffer)

{
	long rsize;

	rsize = dm_read_invis(dmapiSession, handle, handleLength,
						  dmapiToken, offset, size, buffer);

	TRACE(Trace::much, offset);
	TRACE(Trace::much, size);
	TRACE(Trace::much, rsize);

	return rsize;
}

long FsObj::write(long offset, unsigned long size, char *buffer)

{
	long wsize;

	wsize = dm_write_invis(dmapiSession, handle, handleLength, dmapiToken,
						   DM_WRITE_SYNC, offset, size, buffer);
	TRACE(Trace::much, offset);
	TRACE(Trace::much, size);
	TRACE(Trace::much, wsize);

	return wsize;
}


void FsObj::addAttribute(mig_attr_t value)

{
	int rc;

	value.typeId = typeid(mig_attr_t).hash_code();
	value.added = true;

	rc = dm_set_dmattr(dmapiSession, handle, handleLength,
					   dmapiToken, (dm_attrname_t *) Const::DMAPI_ATTR_MIG.c_str(),
					   0, sizeof(mig_attr_t), (void *) &value);

	if ( rc == -1 ) {
		TRACE(Trace::error, errno);
		throw(errno);
	}
}

void FsObj::remAttribute()

{
	int rc;

	rc = dm_remove_dmattr(dmapiSession, handle, handleLength,
						  dmapiToken, 0, (dm_attrname_t *) Const::DMAPI_ATTR_MIG.c_str());

	if ( rc == -1 ) {
		TRACE(Trace::error, errno);
		throw(errno);
	}
}

FsObj::mig_attr_t FsObj::getAttribute()

{
	int rc;
	unsigned long rsize;
	FsObj::mig_attr_t attr;

	memset(&attr, 0, sizeof(mig_attr_t));

	rc = dm_get_dmattr(dmapiSession, handle, handleLength, DM_NO_TOKEN,
					   (dm_attrname_t *) Const::DMAPI_ATTR_MIG.c_str(), sizeof(attr), &attr, &rsize);

	if ( rc == -1 && errno == ENOENT ) {
		attr.added = false;
		return attr;
	}
	else if ( rc == -1 ) {
		TRACE(Trace::error, errno);
		throw(errno);
	}
	else if ( attr.typeId != typeid(attr).hash_code() ) {
		TRACE(Trace::error, attr.typeId);
		throw(Error::LTFSDM_ATTR_FORMAT);
	}

	return attr;
}

void FsObj::preparePremigration()

{
	int rc;
	dm_region_t reg;
	dm_boolean_t exact;


	/* Always remove entire file */
	reg.rg_offset = 0;
	reg.rg_size = 0;         /* 0 = infinity */

	/* Mark the region as premigrated */
	reg.rg_flags = DM_REGION_WRITE | DM_REGION_TRUNCATE;
	rc = dm_set_region(dmapiSession, handle, handleLength, dmapiToken, 1, &reg, &exact);

	if ( rc == -1 ) {
		TRACE(Trace::error, errno);
		throw(errno);
	}
}

void FsObj::finishPremigration()

{
	/*
	 * premigration is currently set in preparePremigration which
	 * should happen here
	 */
}

void FsObj::prepareRecall()

{
}

void FsObj::finishRecall(FsObj::file_state fstate)

{
	int rc;
	dm_region_t reg;
	dm_boolean_t exact;

	if ( fstate == FsObj::PREMIGRATED ) {
		/* Always remove entire file */
		reg.rg_offset = 0;
		reg.rg_size = 0;;         /* 0 = infinity */

		/* Mark the region as off-line */
		reg.rg_flags = DM_REGION_WRITE | DM_REGION_TRUNCATE;
		rc = dm_set_region(dmapiSession, handle, handleLength,
						   dmapiToken, 1, &reg, &exact);

		if ( rc == -1 ) {
			TRACE(Trace::error, errno);
			throw(errno);
		}
	}
	else {
		memset(&reg, 0, sizeof(reg));
		reg.rg_flags = DM_REGION_NOEVENT;

		rc = dm_set_region(dmapiSession, handle, handleLength,
						   dmapiToken, 0, &reg, &exact);

		if ( rc == -1 ) {
			TRACE(Trace::error, errno);
			throw(errno);
		}
	}

}


void FsObj::prepareStubbing()

{
	int rc;
	dm_region_t reg;
	dm_boolean_t exact;


	/* Always remove entire file */
	reg.rg_offset = 0;
	reg.rg_size = 0;;         /* 0 = infinity */

	/* Mark the region as off-line */
	reg.rg_flags = DM_REGION_READ | DM_REGION_WRITE | DM_REGION_TRUNCATE;
	rc = dm_set_region(dmapiSession, handle, handleLength, dmapiToken, 1, &reg, &exact);

	if ( rc == -1 ) {
		TRACE(Trace::error, errno);
		throw(errno);
	}
}


void FsObj::stub()

{
	int rc;

	/* Remove online data within the region */
	rc = dm_punch_hole(dmapiSession, handle, handleLength, dmapiToken, 0, 0);

	if ( rc == -1 ) {
		TRACE(Trace::error, errno);
		throw(errno);
	}
}


FsObj::file_state FsObj::getMigState()
{
    unsigned int nelem = 2;
    dm_region_t regbuf[2];
	std::stringstream infos;
	int rc;
	unsigned int i;
	FsObj::mig_attr_t attr;

	attr = getAttribute();

	if ( attr.added == false )
		return FsObj::RESIDENT;

    memset(regbuf, 0, sizeof( regbuf ));

    rc = dm_get_region(dmapiSession, handle, handleLength,
					   DM_NO_TOKEN, nelem, regbuf, &nelem);

	if ( rc == -1 ) {
		TRACE(Trace::error, errno);
		throw(errno);
    }

    for (i = 0; i < nelem; i++ ) {
		infos << "region nr: " << i << ", offset: " << regbuf[i].rg_offset
			  << ", size: " << regbuf[i].rg_size << ", flag: " << regbuf[i].rg_size;
		TRACE(Trace::much, infos.str());
	}

	if ( nelem > 1 ) {
		TRACE(Trace::error, nelem);
		throw(nelem);
    }

	if ( nelem == 0 ) {
		return FsObj::RESIDENT;
	}
	else if ( regbuf[0].rg_flags == (DM_REGION_READ | DM_REGION_WRITE | DM_REGION_TRUNCATE) ) {
		return FsObj::MIGRATED;
	}
	else if ( regbuf[0].rg_flags == ( DM_REGION_WRITE | DM_REGION_TRUNCATE) ) {
		return FsObj::PREMIGRATED;
	}
	else {
		TRACE(Trace::error, regbuf[0].rg_flags);
		throw(Error::LTFSDM_GENERAL_ERROR);
	}
}
