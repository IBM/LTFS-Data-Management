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

Connector::Connector()

{
	char          *version          = NULL;
    // size_t         msglen           = 8;
    // char           msgdatap[8];


	//    memset((char *) msgdatap, 0, sizeof(msgdatap));

    if (dm_init_service(&version)) {
		TRACE(Trace::error, errno);
		goto failed;
	}

	if (dm_create_session(DM_NO_SESSION, (char *) "ltfsdm", &dmapiSession)) {
		TRACE(Trace::error, errno);
		goto failed;
	}

    // if(dm_create_userevent(dmapiSession, msglen, (void*)msgdatap, &dmapiToken)) {
    //     dm_destroy_session(dmapiSession);
	// 	TRACE(Trace::error, errno);
	// 	goto failed;
	// }

	return;

failed:
	MSG(LTFSDMS0016E);
}

Connector::~Connector()

{
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
