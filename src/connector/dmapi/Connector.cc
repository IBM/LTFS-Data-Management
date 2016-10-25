#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>

#include <string>
#include <sstream>

#include "src/common/util/util.h"
#include "src/common/messages/Message.h"
#include "src/common/tracing/Trace.h"
#include "src/common/errors/errors.h"
#include "src/common/const/Const.h"

#include <xfs/dmapi.h>

#include "src/connector/Connector.h"

dm_sessid_t dmapiSession;
dm_token_t dmapiToken;

void dmapiSessionCleanup()

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
		throw(LTFSDMErr::LTFSDM_GENERAL_ERROR);
	}

	while ( dm_getall_sessions(num_sessions, sidbufp, &num_sessions_res) == -1 ) {
        free(sidbufp);

        if ( errno != E2BIG ) {
            TRACE(Trace::error, errno);
			throw(LTFSDMErr::LTFSDM_GENERAL_ERROR);
        }

		sidbufp = NULL;
        sidbufp = (dm_sessid_t*)malloc(num_sessions_res * sizeof( dm_sessid_t ));

		if ( !sidbufp ) {
            MSG(LTFSDMD0001E);
			throw(LTFSDMErr::LTFSDM_GENERAL_ERROR);
        }

		num_sessions = num_sessions_res;
    }

	MSG(LTFSDMD0002I, num_sessions);

	unsigned int num_tokens = 1024;
	dm_token_t *tokbufp = NULL;

	for ( i = 0; i < (int)num_sessions; i++ ) {
        if ( dm_query_session(sidbufp[i], sizeof( buffer ), buffer, &rseslenp) == -1 ) {
            MSG(LTFSDMD0001E);
			free(sidbufp);
			throw(LTFSDMErr::LTFSDM_GENERAL_ERROR);
        }
		else if (Const::DMAPI_SESSION_NAME.compare(buffer) == 0) {
			TRACE(Trace::error, i);
			TRACE(Trace::error, (unsigned long) sidbufp[i]);

			tokbufp =  (dm_token_t *) malloc(sizeof(dm_token_t) * num_tokens);
			if ( !tokbufp ) {
				MSG(LTFSDMD0001E);
				free(sidbufp);
				throw(LTFSDMErr::LTFSDM_GENERAL_ERROR);
			}

			while (dm_getall_tokens(sidbufp[i], num_tokens, tokbufp, &rtoklenp) == -1) {
				if ( errno != E2BIG ) {
					TRACE(Trace::error, errno);
					throw(LTFSDMErr::LTFSDM_GENERAL_ERROR);
				}
				free(tokbufp);
				tokbufp = NULL;
				tokbufp =  (dm_token_t *) malloc(sizeof(dm_token_t) * rtoklenp);
				if ( !tokbufp ) {
					MSG(LTFSDMD0001E);
					free(sidbufp);
					throw(LTFSDMErr::LTFSDM_GENERAL_ERROR);
				}
			}

			for (j = 0; j<rtoklenp; j++)
			{
				TRACE(Trace::error, j);
				TRACE(Trace::error, (unsigned long) tokbufp[j]);
				if ( dm_respond_event(sidbufp[i], tokbufp[j], DM_RESP_ABORT, EINTR, 0, NULL) == 1 )
					TRACE(Trace::error, errno);
				else
					MSG(LTFSDMD0003I);
			}

			if ( dm_destroy_session(sidbufp[i]) == -1 ) {
				TRACE(Trace::error, errno);
				MSG(LTFSDMD0004E);
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

Connector::Connector()

{
	char          *version          = NULL;
    size_t         msglen           = 8;
    char           msgdatap[8];

	memset((char *) msgdatap, 0, sizeof(msgdatap));

    if (dm_init_service(&version)) {
		TRACE(Trace::error, errno);
		goto failed;
	}

	dmapiSessionCleanup();

	if (dm_create_session(DM_NO_SESSION, (char *) Const::DMAPI_SESSION_NAME.c_str(), &dmapiSession)) {
		TRACE(Trace::error, errno);
		goto failed;
	}

    if(dm_create_userevent(dmapiSession, msglen, (void*)msgdatap, &dmapiToken)) {
        dm_destroy_session(dmapiSession);
		TRACE(Trace::error, errno);
		goto failed;
	}

	return;

failed:
	MSG(LTFSDMS0016E);
}

Connector::~Connector()

{
	if ( dm_respond_event(dmapiSession, dmapiToken, DM_RESP_ABORT, EINTR, 0, NULL) == 1 )
		TRACE(Trace::error, errno);
	else
		MSG(LTFSDMD0003I);

	dm_destroy_session(dmapiSession);
}

FsObj::FsObj(std::string fileName) : handle(NULL), handleLength(0)

{
	char *fname = (char *) fileName.c_str();

	if ( dm_path_to_handle(fname, &handle, &handleLength) != 0 ) {
		TRACE(Trace::error, errno);
		throw(errno);
	}
}

FsObj::FsObj(unsigned long long fsId, unsigned int iGen, unsigned long long iNode) : handle(NULL), handleLength(0)

{
	if ( dm_make_handle(&fsId, &iNode, &iGen, &handle, &handleLength) != 0 ) {
		TRACE(Trace::error, errno);
		throw(errno);
	}
}

FsObj::~FsObj()

{
	dm_handle_free(handle, handleLength);
}

struct stat FsObj::stat()

{
	struct stat statbuf;
	dm_stat_t dmstatbuf;

	memset(&statbuf, 0, sizeof(statbuf));

	if ( handle == NULL )
		return statbuf;

 	if ( dm_get_fileattr(dmapiSession, handle, handleLength, DM_NO_TOKEN, DM_AT_STAT, &dmstatbuf) != 0 ) {
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
