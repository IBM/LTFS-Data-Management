#include <unistd.h>
#include <signal.h>
#include <fcntl.h>
#include <errno.h>

#include <string>

#include "src/common/messages/Message.h"
#include "src/common/tracing/Trace.h"
#include "src/common/errors/errors.h"
#include "src/common/const/Const.h"

#include "src/server/ServerComponent/ServerComponent.h"
#include "src/server/SubServer/SubServer.h"
#include "src/server/Server.h"


int main(int argc, char **argv)

{
 	Server ltfsdmd;
	OLTFSErr err = OLTFSErr::OLTFS_OK;

	TRACE(Trace::little, getpid());

	try {
		ltfsdmd.initialize();
		ltfsdmd.daemonize();
		ltfsdmd.run();
	}
	catch ( OLTFSErr initerr ) {
		err = initerr;
		goto end;
	}

end:
	return (int) err;
}
