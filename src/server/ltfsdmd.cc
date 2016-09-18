#include <unistd.h>
#include <signal.h>
#include <fcntl.h>
#include <errno.h>

#include <string>

#include "src/common/messages/messages.h"
#include "src/common/tracing/trace.h"
#include "src/common/errors/errors.h"
#include "src/common/const/Const.h"

#include "src/server/Server.h"


int main(int argc, char **argv)

{
 	Server ltfsdmd;
	OLTFSErr err = OLTFSErr::OLTFS_OK;

	try {
		ltfsdmd.initialize();
		ltfsdmd.daemonize();
	}
	catch ( OLTFSErr initerr ) {
		err = initerr;
		goto end;
	}

	while (true) {};

end:
	return (int) err;
}
