#include <unistd.h>
#include <signal.h>
#include <fcntl.h>
#include <limits.h>
#include <errno.h>

#include <string>
#include <condition_variable>
#include <thread>

#include "src/common/util/util.h"
#include "src/common/messages/Message.h"
#include "src/common/tracing/Trace.h"
#include "src/common/errors/errors.h"
#include "src/common/const/Const.h"

#include "src/connector/Connector.h"
#include "src/server/SubServer.h"
#include "src/server/Server.h"


int main(int argc, char **argv)

{
 	Server ltfsdmd;
	int err = LTFSDMErr::LTFSDM_OK;
	bool detach = true;
	int opt;

	opterr = 0;

	while (( opt = getopt(argc, argv, "f")) != -1 ) {
		switch( opt ) {
			case 'f':
				detach = false;
				break;
			default:
				std::cerr << messages[LTFSDMC0013E] << std::endl;
				err = LTFSDMErr::LTFSDM_GENERAL_ERROR;
				goto end;
		}
	}

	try {
		LTFSDM::init();
	}
	catch(...) {
		err = LTFSDMErr::LTFSDM_GENERAL_ERROR;
		goto end;
	}

	traceObject.setTrclevel(Trace::much);
	TRACE(Trace::little, getpid());

	try {
		ltfsdmd.initialize();
		if ( detach )
			ltfsdmd.daemonize();
		ltfsdmd.run();
	}
	catch ( int initerr ) {
		err = initerr;
		goto end;
	}

end:
	return (int) err;
}
