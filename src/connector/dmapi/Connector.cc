#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>

#include <string>

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

FsObj::FsObj(std::string fileName)

{
	char *fname = (char *) fileName.c_str();

	if ( dm_path_to_handle(fname, &handle, &size) != 0 ) {
		TRACE(Trace::error, errno);
		throw(errno);
	}
}

FsObj::FsObj(unsigned long long fsId, unsigned int iGen, unsigned long long iNode)

{
	if ( dm_make_handle(&fsId, &iNode, &iGen, &handle, &size) != 0 ) {
		TRACE(Trace::error, errno);
		throw(errno);
	}
}

FsObj::~FsObj()

{
	dm_handle_free(handle, size);
}

struct stat FsObj::stat()

{
	struct stat statbuf;
	dm_stat_t dmstatbuf;

	memset(&statbuf, 0, sizeof(statbuf));

 	if ( dm_get_fileattr(dmapiSession, handle, size, DM_NO_TOKEN, DM_AT_STAT, &dmstatbuf) != 0 ) {
		TRACE(Trace::error, errno);
	    throw(errno);
	}



	return statbuf;
}

unsigned long FsObj::getFsId()

{
	return 11;
}

unsigned long FsObj::getIGen()

{
	return 12;
}

unsigned long FsObj::getINode()

{
	return 13;
}
