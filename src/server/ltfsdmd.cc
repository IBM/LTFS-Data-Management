#include <unistd.h>
#include <signal.h>
#include <fcntl.h>
#include <errno.h>

#include <string>

#include "src/common/util/util.h"
#include "src/common/messages/Message.h"
#include "src/common/tracing/Trace.h"
#include "src/common/errors/errors.h"
#include "src/common/const/Const.h"

#include "src/server/SubServer/SubServer.h"
#include "src/server/Server.h"


int main(int argc, char **argv)

{
 	Server ltfsdmd;
	LTFSDMErr err = LTFSDMErr::LTFSDM_OK;
	bool detach = true;
	int opt;

	opterr = 0;

	while (( opt = getopt(argc, argv, "b")) != -1 ) {
		switch( opt ) {
			case 'b':
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


	TRACE(Trace::little, getpid());

	try {
		ltfsdmd.initialize();
		if ( detach )
			ltfsdmd.daemonize();
		ltfsdmd.run();
	}
	catch ( LTFSDMErr initerr ) {
		err = initerr;
		goto end;
	}

end:
	return (int) err;
}
